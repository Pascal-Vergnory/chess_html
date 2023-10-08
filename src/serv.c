#include <stdio.h>
#include <string.h>
#include <pthread.h>

#ifdef __MINGW32__ // Windows

#include <winsock2.h>
#include <windows.h>
#define socklen_t int
#pragma comment(lib, "ws2_32.lib") // Winsock Library

#else              // Linux

#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define INVALID_SOCKET -1
#define SOCKET int

#endif

#include "engine.h"

//-----------------------------------------------------------------------------
// The function to send files such as index.html, css and images to the client
//-----------------------------------------------------------------------------

static void send_file( SOCKET* client, char *name)
{
    char header[128];
    char type[16];

    // No name means index.html is requested
    if (name[0] == 0) strcpy( name, "index.html");

    printf("file name: %s\n", name);

    // get file extension (by default plain text)
    strcpy(type, "text/plain");
    const char *dot = strrchr(name, '.');
    if (dot > name) {
        dot++;
        if (strcmp(dot, "html") == 0) strcpy(type, "text/html");
        if (strcmp(dot, "png") == 0)  strcpy(type, "image/png");
        if (strcmp(dot, "css") == 0)  strcpy(type, "text/css");
        if (strcmp(dot, "svg") == 0)  strcpy(type, "image/svg+xml");
    }

    // Read file in a buffer
    FILE *f = fopen(name, "rb");
    if (f == NULL) {
        strcpy( header, "HTTP/1.1 404 Not Found\r\n\r\n");
        printf( "-->\n%s\n", header);
        send( *client, header, strlen(header), 0);
        return;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *file_buf = malloc(fsize + 1);
    fsize = fread(file_buf, 1, fsize, f);
    fclose(f);
    file_buf[fsize] = 0;

    // Send the HTTP header
    sprintf( header, "HTTP/1.0 200 OK\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %ld\r\n\r\n", type, fsize);
    printf( "-->\n%s\n", header);
    send( *client, header, strlen(header), 0);

    // Send the file
    send( *client, file_buf, fsize, 0);
    free(file_buf);
}

//-----------------------------------------------------------------------------
// Conversion functions to interface with engine.c
//-----------------------------------------------------------------------------

void board_string_to_fen( char* board, char *fen)
{
    int i, f = 0, empties = 0;

    for (i = 0; i < 64; i++) {
        char c = board[i];
        if (i && i%8 == 0) {
            if (empties) {
                fen[f++] = '0' + empties;
                empties = 0;
            }
            fen[f++] = '/';
        }
        if (c == '_') empties++;
        else {
            if (empties) {
                fen[f++] = '0' + empties;
                empties = 0;
            }
            fen[f++] = c;
        }
    }
    while( board[i] != 0 && board[i] != ' ') {
        char c = board[i++];
        fen [f++] = c == '_' ? ' ' : c;
    }
    fen[f] = 0;
}

static void update_board_string( char* ptr )
{
    for (int l=0; l<8; l++) {
        for (int c=0; c<8; c++) {
             char ch = get_piece(7-l,c);
             *ptr++ = (ch == ' ') ? '_' : ch;
        }
    }
}

void log_info( const char* str ) { fputs( str, stdout ); }
void send_str( const char* str ) { fputs( str, stdout ); }

//-----------------------------------------------------------------------------
// The function to serve the client requests
//-----------------------------------------------------------------------------

static void* serve( void* void_client )
{
    SOCKET* client = void_client;
    int ret = 0;
    char header[128] = { 0 };
    char buffer[1024] = { 0 };
    char* name  = buffer +  5;  // After "GET /"
    char* move  = buffer +  9;  // After "GET /cmd_"
    char* board = buffer + 14;  // After "GET /cmd_e2e4_"

    // get next HTTP request
    int l = recv( *client, buffer, sizeof(buffer) - 1, 0);
    buffer[l] = 0;
    printf( "<--\n%s\n", buffer);

    // Get name argument of GET
    char *space = strchr( name, ' ');
    if (space >= name) *space = 0;

    // Check if the request is an interaction with the chess engine
    if (strncmp(name, "cmd_", 4) == 0) {

        // Provide the current game state to the chess engine
        static char fen[100];
        board_string_to_fen( board, fen);
        init_game( fen );

        // Let the engine play
        if (strncmp( move, "play", 4) == 0) compute_next_move();

        else if (strncmp( move, "dp", 2) == 0) {
            set_possible_moves_board( move[3]-'1', move[2]-'a' );
            for( int i = 0; i < 64; i++ )
                board[i] = get_possible_moves_board( 7- (i/8), i%8 ) ? '1' : '_';
            board[64] = 0;
            goto send_header_and_board;
        }

        // Verify the move and apply it if it is legal
        else ret = try_move_str(move);

        // Handle a move that promotes a pawn
        if (ret == 2 && move[4]=='_') {
            user_undo_move();
            board[64]='P';       // Give a sign to the client that the move promotes a pawn
        }
        else board[64]='_';

        // Rebuild the full board string
        update_board_string( board );
        set_FEN_end( board + 65, '_' ); 

send_header_and_board:
        // Send the HTTP header
        sprintf( header, "HTTP/1.0 200 OK\r\n"
                         "Content-Type: text/plain\r\n"
                         "Access-Control-Allow-Origin: *\r\n"
                         "Content-Length: %d\r\n\r\n", (int)strlen(board));
        printf("-->\n%s\n", header);
        send( *client, header, strlen(header), 0);

        // Send the response: the updated (or not) board state
        printf("-->\n%s\n", board);
        send( *client, board, strlen(board), 0);
    }

    // Otherwise consider the request as a file requests
    else send_file( client, name);

#ifdef __MINGW32__ // Windows
    closesocket( *client );
#else              // Linux
    shutdown( *client, SHUT_RDWR);
    close( *client );
#endif
    *client = INVALID_SOCKET;
    return NULL;
}

//-----------------------------------------------------------------------------
// The main entry point. Setup the server socket and listen to clients
//-----------------------------------------------------------------------------

#define NB_CLIENTS_MAX 16
int main( void )
{
    SOCKET server_socket, clients[NB_CLIENTS_MAX];
    pthread_t serve_threads[NB_CLIENTS_MAX];
    struct sockaddr_in server, client;
    int c, s;

#ifdef __MINGW32__ // Windows
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Winsock init failed. Error Code : %d", WSAGetLastError());
        return 1;
    }
#endif

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Could not create the server socket\n");
        return 1;
    }

#ifndef __MINGW32__ // Windows
    // Set these options to bind successfully even just after having stopped
    const int enable = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        printf("setsockopt(SO_REUSEADDR) failed");
        return 1;
    }
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0) {
        printf("setsockopt(SO_REUSEPORT) failed");
        return 1;
    }
#endif

    // Serve on localhost port 2320 (http://localhost:2320)
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(2320);

    // Bind the socket
    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) != 0) {
        printf("Server socket bind failed\n");
        return 1;
    }
    printf("Server listening to http://localhost:2320\n");

    // Listen to incoming connections
    listen(server_socket, 3);

    // Start with all client sockets available
    for (c = 0; c < NB_CLIENTS_MAX; c++) clients[c] = INVALID_SOCKET;
    c = 0;

    // Start a web browser that will browse on us !
#ifdef __MINGW32__ // Windows
    s = system("\"C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome\" --app=http://localhost:2320> NUL 2>&1");
#else              // Linux
    s = system("chromium --app=http://localhost:2320 >/dev/null 2>&1 &");
#endif
    printf("Browser spawn result: %d\n", s);
    
    while (1) {
        // Find an available clients sockets
        while (clients[c] != INVALID_SOCKET) c = (c + 1) % NB_CLIENTS_MAX;

        // Accept an incoming connection
        socklen_t sz = sizeof(struct sockaddr_in);
        clients[c] = accept(server_socket, (struct sockaddr *)&client, &sz);
        if (clients[c] == INVALID_SOCKET) printf("accept failed\n");
        else {
//            printf("Client %s:%u accepted; Running thread %d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port), c);

            // Serve the connection
#ifdef __MINGW32__ // Windows
            serve( (void *) &clients[c] );
#else              // Linux
            pthread_create( &serve_threads[c], NULL, serve, (void *) &clients[c]);
#endif
        }
    }

#ifdef __MINGW32__ // Windows
    closesocket(server_socket);
    WSACleanup();
#else              // Linux
    shutdown( server_socket, SHUT_RDWR);         //All further send and recieve operations are DISABLED...
    close( server_socket );
#endif
    return 0;
}
