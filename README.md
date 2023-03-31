# chess_html

It is the same small chess engine as chess, but it uses your WEB browser as graphical user interface !

Why ?
- A WEB browser is a powerfull "Graphical User Interface", so lets use it !
- If my program uses a browser for its interface, then I need no complicated graphical library.
- I wanted to learn how a WEB server and a WEB page work

So...

## Pre-requisites to build

On Linux, gcc must be installed.  
On Windows, msys64 and MINGW64 must be installed.

## Building chess or chess.exe

On Linux run ./build (no argument needed).  
On Windows run build.bat (no argument needed).

## Using chess (Linux), or chess.exe (Windows)

The files chess (or chess.exe on Windows), index.html and Chess_Pieces.svg must be in a same folder

Launch ./chess (Linux) or chess.exe (Windows). This will start the chess engine as a WEB server answering at address http://localhost:2320. It will also try to spawn Google Chrome (in app mode) on this address. You should see the graphical interface appear! If it did not succeed to spawn Google Chrome (because may-be you don't have Google Chrome, or it is not at its default path), then you need to open your favorite browser, and type in the navigation bar the address http://localhost:2320. Or a faster way is to double-click on the file index.html.

## Source code

The "purely" chess engine functions are in src/engine.c (more information on the chess engine in the project 'chess').  
The WEB server logic is in src/serv.c.  
You can get an idea of the "chess API" interface between serv.c and engine.c in engine.h.  
The WEB page layout and the javascript code to manage the interactions with the WEB server are both in index.html. All the graphical work is done, using the html "canvas' framework.  
The chess pieces vectorized drawings are in Chess_Pieces.svg (it's a text file, you can take a look inside!)

That's it! No other dependencies than the basic GCC libraries!
