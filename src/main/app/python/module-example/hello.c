/*
* hello.c - Hello, World! Como um modulo do Kernel
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

/*
* hello_init - A funcao init, chamada quando o modulo eh carregado.
* Retorna zero se carregado com sucesso, e nao-zero caso contrario.
*/

static int hello_init(void)
{
        printk(KERN_ALERT "Hello World!\n");
        return 0;
}

/*
* hello_exit - A Funcao de saida, chamada quando o modulo eh removido.
*/

static void hello_exit(void)
{
        printk(KERN_ALERT "Bye World!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aluno");
