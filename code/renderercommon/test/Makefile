COMPILE = gcc -Wall


INCLUDE_DIR = ..
CFLAGS = -O3 -funroll-loops -fomit-frame-pointer -msse2 -ffast-math -I $(INCLUDE_DIR)


ALL: clean test_matrix_multiplication test_matrix_transform test_Mat4Copy


test_matrix_multiplication: test_matrix_multiplication.c ../matrix_multiplication.c
	$(COMPILE) test_matrix_multiplication.c ../matrix_multiplication.c -o test_matrix_multiplication -x assembler\-with\-cpp MatrixMultiply4x4_ASM.s $(CFLAGS)

test_matrix_transform: test_matrix_transform.c ../matrix_multiplication.c
	$(COMPILE) test_matrix_transform.c ../matrix_multiplication.c -o test_matrix_transform  $(CFLAGS)

test_Mat4Copy: 
	$(COMPILE) test_Mat4Copy.c -o test_Mat4Copy $(CFLAGS)

clean:
	rm -f *.o test_matrix_multiplication test_Mat4Copy 
#main:main.o
#	$(COMPILE) -o $(TARGETS) main.o
#.c.o:
#	$(COMPILE) -c $< -o $@
#make
