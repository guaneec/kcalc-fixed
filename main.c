#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/string.h> /* for memset() and memcpy() */
#include "expression.h"
#include "fixed-point.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Math expression evaluation");
MODULE_VERSION("0.1");

#define DEVICE_NAME "calc"
#define CLASS_NAME "calc"
#define BUFF_SIZE 256

#define PRIu64 "llu"

static int major;
static char message[BUFF_SIZE] = {0};
static short size_of_message;
static struct class *char_class = NULL;
static struct device *char_dev = NULL;
static char *msg_ptr = NULL;
static uint64_t result = 0;

/* The prototype functions for the character driver */
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static void calc(void);

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

static int dev_open(struct inode *inodep, struct file *filep)
{
    msg_ptr = message;
    return 0;
}

static ssize_t dev_read(struct file *filep,
                        char *buffer,
                        size_t len,
                        loff_t *offset)
{
    int error_count = 0;

    if (*msg_ptr == 0)
        return 0;

    memset(message, 0, sizeof(char) * BUFF_SIZE);

    snprintf(message, 64, "%" PRIu64 "\n", (unsigned long long) result);
    size_of_message = strlen(message);

    error_count = copy_to_user(buffer, message, size_of_message);
    if (error_count == 0) {
        pr_info("size: %d result: %" PRIu64 "\n", size_of_message, result);
        while (len && *msg_ptr) {
            error_count = put_user(*(msg_ptr++), buffer++);
            len--;
        }

        if (error_count == 0)
            return (size_of_message);
        return -EFAULT;
    } else {
        pr_info("Failed to send %d characters to the user\n", error_count);
        return -EFAULT;
    }
}

static ssize_t dev_write(struct file *filep,
                         const char *buffer,
                         size_t len,
                         loff_t *offset)
{
    memset(message, 0, sizeof(char) * BUFF_SIZE);

    if (len >= BUFF_SIZE) {
        pr_alert("Expression too long");
        return 0;
    }

    copy_from_user(message, buffer, len);
    pr_info("Received %ld -> %s\n", len, message);

    calc();
    return len;
}

noinline void user_func_nop_cleanup(struct expr_func *f, void *c)
{
    /* suppress compilation warnings */
    (void) f;
    (void) c;
}

noinline uint64_t user_func_nop(struct expr_func *f, vec_expr_t args, void *c)
{
    (void) args;
    (void) c;
    if (f->ctxsz == 0)
        return -1;
    return 0;
}

// https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Binary_numeral_system_(base_2)
// should be accurate to the least significant bit
uint64_t fpsqrt(uint64_t n)
{
    int offset = n < 1ull << 48 ? 8 : 0;
    n <<= offset * 2;
    n >>= 15;
    uint64_t x = 0;
    uint64_t s = 0;
    uint64_t b = 1ull << 47;
    while (b >= 1ull << offset) {
        uint64_t t = s + b;
        if (n >= t) {
            n -= t;
            s = t + b;
            x += b;
        }
        n <<= 1;
        b >>= 1;
    }
    return x >> offset;
}

noinline uint64_t user_func_sqrt(struct expr_func *f, vec_expr_t args, void *c)
{
    (void) args;
    (void) c;
    if (1 != args.len)
        return NAN_INT;
    uint64_t x = expr_eval(&args.buf[0]);
    if (x == NAN_INT || x == INF_INT)
        return x;
    if ((int64_t) x < 0)
        return NAN_INT;
    return fpsqrt(x);
}

noinline uint64_t user_func_sum(struct expr_func *f, vec_expr_t args, void *c)
{
    // args: summand, counter, start, end
    // see also: http://eagle.cs.kent.edu/MAXIMA/maxima_6.html#IDX171
    (void) args;
    (void) c;
    if (4 != args.len)
        return NAN_INT;
    if (args.buf[1].type != OP_VAR)
        return NAN_INT;
    int64_t *i = (int64_t *) args.buf[1].param.var.value;
    uint64_t sum = 0;
    int64_t start = expr_eval(&args.buf[2]);
    int64_t stop = expr_eval(&args.buf[3]);
    int64_t safety = (1l << 32) - 1;
    for (*i = start; *i <= stop && safety >= 0; *i += (1ll << 32), --safety) {
        uint64_t d = expr_eval(&args.buf[0]);
        if (d == NAN_INT || d == INF_INT)
            return d;
        sum += d;

        // overflow detection
        if ((int64_t)(*(uint64_t *) i + (1ul << 32)) < *i)
            break;
    }
    if (safety < 0)
        return NAN_INT;
    return sum;
}

static struct expr_func user_funcs[] = {
    {"nop", user_func_nop, user_func_nop_cleanup, 0},
    {"sqrt", user_func_sqrt, user_func_nop_cleanup, 0},
    {"sum", user_func_sum, user_func_nop_cleanup, 0},
    {NULL, NULL, NULL, 0},
};

static void calc(void)
{
    struct expr_var_list vars = {0};
    struct expr *e = expr_create(message, strlen(message), &vars, user_funcs);
    if (!e) {
        pr_alert("Syntax error");
        return;
    }

    result = expr_eval(e);
    pr_info("Result: %" PRIu64 "\n", result);
    expr_destroy(e, &vars);
}

static int dev_release(struct inode *inodep, struct file *filep)
{
    pr_info("Device successfully closed\n");
    return 0;
}

static int __init calc_init(void)
{
    pr_info("Initializing the module\n");

    /* Try to dynamically allocate a major number for the device -- more
     * difficult but worth it
     */
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        pr_alert("Failed to register a major number\n");
        return major;
    }
    pr_info("registered correctly with major number %d\n", major);

    /* Register the device class */
    char_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(char_class)) {  // Check for error and clean up if there is
        unregister_chrdev(major, DEVICE_NAME);
        pr_alert("Failed to register device class\n");
        return PTR_ERR(char_class); /* return an error on a pointer */
    }
    pr_info("device class registered correctly\n");

    /* Register the device driver */
    char_dev =
        device_create(char_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(char_dev)) {        /* Clean up if there is an error */
        class_destroy(char_class); /* Repeated code but the alternative is
                                    * goto statements
                                    */
        unregister_chrdev(major, DEVICE_NAME);
        pr_alert("Failed to create the device\n");
        return PTR_ERR(char_dev);
    }
    pr_info("device class created correctly\n");
    return 0;
}

static void __exit calc_exit(void)
{
    device_destroy(char_class, MKDEV(major, 0)); /* remove the device */
    class_unregister(char_class);          /* unregister the device class */
    class_destroy(char_class);             /* remove the device class */
    unregister_chrdev(major, DEVICE_NAME); /* unregister the major number */
    pr_info("Goodbye!\n");
}

module_init(calc_init);
module_exit(calc_exit);
