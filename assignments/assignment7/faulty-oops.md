# Assignment 7: Part 2

Faulty Oops Analysis

This oops is from attempting to writing to the `faulty` kernel driver. The oops can be reproduced by running `echo "hello_world" > /dev/faulty` in the Assignment 7 Part 2 qemu environment.

## Full Trace

The full trace of the oops, below, is verbatim from the kernel output.

```
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000042111000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: hello(O) faulty(O)
CPU: 0 PID: 121 Comm: sh Tainted: G           O      5.15.18 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xa8/0x2b0
sp : ffffffc008c83d80
x29: ffffffc008c83d80 x28: ffffff80020e2640 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040001000 x22: 0000000000000012 x21: 0000005591b42a70
x20: 0000005591b42a70 x19: ffffff80020aae00 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006f0000 x3 : ffffffc008c83df0
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x14/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x40/0xa0
 el0_svc+0x20/0x60
 el0t_64_sync_handler+0xe8/0xf0
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 366a2cdb701ac609 ]---
```

## Analysis

_The following breakdown will summarize the oops message, section by section._

```
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
```
The type of the oops. The kernel could not dereference a null pointer.

<hr>

```
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
```
Info about the abort from the Exception Syndrome Register (ESR). See the [patch where this was initially added](https://patchwork.kernel.org/project/linux-arm-kernel/patch/1497446043-50944-1-git-send-email-julien.thierry@arm.com/) for details.

<hr>

```
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000042111000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
```
More metainfo, the err number (96000045), and the die counter (#1).

<br>

```
Modules linked in: hello(O) faulty(O)
```
The modules linked at the time of the error. May be a good indication og what modules _caused_ the error.

<hr>

```
CPU: 0 PID: 121 Comm: sh Tainted: G           O      5.15.18 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
```
Some additional metainfo about the runtume, including: The CPU ID (0), pid (121), the process running at the time (sh), the kernel state (tainted), and kernel version (5.15.18), as well as hardware info and the [pstate](https://developer.arm.com/documentation/100933/0100/Processor-state-in-exception-handling) during the oops.

<hr>

```
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xa8/0x2b0

```
The program counter and link register are the first hints as to where our crash happened. The program counter `pc` points to an address in `faulty`, specifically in the `faulty_write` function at offset `0x14` / (20).

If we had enabled debug symbols, we could use gdb to calculate show us the correct offset in the function. However, as the offset is not large we can just take a look at the function itself.

```c
/* faulty.c */
ssize_t
faulty_write (struct file *filp, const char __user *buf, size_t count,
		      loff_t *pos)
{
	/* make a simple fault by dereferencing a NULL pointer */
	*(int *)0 = 0;
	return 0;
}
```

Now, the above comment aside – as that makes it _very_ clear what the error is – we're explicitly attempting to dereference `(int *)0`, i.e. `0` as an int pointer, a null pointer.

The link register `lr` shows the return address will be to the `vfs_write` function. If we did not have such a clearly isolated fault, we could investigate the calling function to get info about the state before calling, the parameters passed to `faulty_write`, etc.

<br>

```
sp : ffffffc008c83d80
x29: ffffffc008c83d80 x28: ffffff80020e2640 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040001000 x22: 0000000000000012 x21: 0000005591b42a70
x20: 0000005591b42a70 x19: ffffff80020aae00 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006f0000 x3 : ffffffc008c83df0
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
```
More register info during the oops.

<br>

```
Call trace:
 faulty_write+0x14/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x40/0xa0
 el0_svc+0x20/0x60
 el0t_64_sync_handler+0xe8/0xf0
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 366a2cdb701ac609 ]---
```
The explicit call trace. If the above inference from the `sp` were insufficient, we could trace here to get information about the calling state, etc.