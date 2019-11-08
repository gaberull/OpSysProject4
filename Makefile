libraries= virtual_disk.o oufs_lib.o storage.o oufs_lib_support.o
CFLAGS = -g -Wall -c
executables = oufs_format oufs_inspect oufs_mkdir oufs_ls oufs_rmdir oufs_stats oufs_touch oufs_append oufs_cat oufs_create oufs_copy oufs_link oufs_remove
includes = oufs.h oufs_lib_support.h storage.h virtual_disk.h oufs_lib.h virtual_disk.h

all: $(executables)

oufs_format: oufs_format.o $(includes) $(libraries)
	gcc oufs_format.o $(libraries) -o oufs_format

oufs_inspect: oufs_inspect.o $(libraries) $(includes)
	gcc oufs_inspect.o $(libraries) -o oufs_inspect

oufs_ls: oufs_ls.o $(includes) $(libraries) 
	gcc oufs_ls.o $(libraries) -o oufs_ls

oufs_mkdir: oufs_mkdir.o $(libraries) $(includes)
	gcc oufs_mkdir.o $(libraries) -o oufs_mkdir

oufs_rmdir: oufs_rmdir.o $(libraries) $(includes)
	gcc oufs_rmdir.o $(libraries) -o oufs_rmdir

oufs_stats: oufs_stats.o $(libraries) $(includes) 
	gcc oufs_stats.o $(libraries) -o oufs_stats

oufs_touch: oufs_stats.o $(libraries) $(includes) 
	gcc oufs_stats.o $(libraries) -o oufs_touch

oufs_append: oufs_stats.o $(libraries) $(includes) 
	gcc oufs_stats.o $(libraries) -o oufs_append

oufs_cat: oufs_stats.o $(libraries) $(includes) 
	gcc oufs_stats.o $(libraries) -o oufs_cat

oufs_create: oufs_stats.o $(libraries) $(includes) 
	gcc oufs_stats.o $(libraries) -o oufs_create

oufs_copy: oufs_stats.o $(libraries) $(includes) 
	gcc oufs_stats.o $(libraries) -o oufs_copy

oufs_link: oufs_stats.o $(libraries) $(includes) 
	gcc oufs_stats.o $(libraries) -o oufs_link

oufs_remove: oufs_stats.o $(libraries) $(includes) 
	gcc oufs_stats.o $(libraries) -o oufs_remove

.c.o:
	gcc $(CFLAGS) $< -o $@

clean:
	rm -f *.o $(executables)

zip: 
	zip project4.zip *.c *.h Makefile README.txt
