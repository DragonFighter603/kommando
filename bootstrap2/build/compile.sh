gcc -g -rdynamic ../main.c ../token.c ../ast.c ../compiler.c ../parser.c ../lib/exit.c ../lib/gc.c ../lib/str.c ../lib/map.c ../lib/debug.c -ldl -o compiler