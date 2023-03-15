inodesimulator: cmd_functions_fs.o  io_fs.o load_struct_fs.o main.o save_struct_fs.o
	gcc cmd_functions_fs.o  io_fs.o load_struct_fs.o main.o save_struct_fs.o -lm -o inodesimulator

cmd_functions_fs.o: cmd_functions_fs.c def_struct_fs.h
	gcc cmd_functions_fs.c -lm -c

io_fs.o: io_fs.c io_fs.h def_struct_fs.h
	gcc io_fs.c -lm -c

load_struct_fs.o: load_struct_fs.c load_struct_fs.h def_struct_fs.h
	gcc load_struct_fs.c -lm -c

main.o: main.c main.h def_struct_fs.h
	gcc main.c -lm -c

save_struct_fs.o: save_struct_fs.c save_struct_fs.h def_struct_fs.h
	gcc save_struct_fs.c -lm -c
	 
