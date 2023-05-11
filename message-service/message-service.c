
#include <linux/module.h>
#include "singlefile-FS/lib/include/singlefilefs.h"
#include "system-call/lib/include/system-call.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luca Fiscariello");

#define MODNAME "Message-Service"

int init_module(void) {

        init_system_call();
        singlefilefs_init();

        return 0;

}

void cleanup_module(void) {

        cleanup_system_calls();
        singlefilefs_exit();
        
}


