#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>


#define VTPMO 156 //this depends on what the kernel tells you when mounting the vtpmo module

int vtpmo(unsigned long x){
	return syscall(VTPMO,x);
}

int main(int argc, char** argv){
        
    vtpmo(1L);
	return 0;

}

