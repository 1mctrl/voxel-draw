CC = clang
CFLAGS = -O2 -Wall -Wextra
LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

voxel: voxel.c
	$(CC) $(CFLAGS) voxel.c -o voxel $(LIBS)

clean:
	rm -f voxel
