# Threads related GDB packets and stuff

Base reference:

  * https://www.sourceware.org/gdb/onlinedocs/gdb.html

## RTOS Symbols

To work with RTOS structures and parse Thread Control Block (TCB) to take thread information, stacks, and stacked
registers we need to take access to the RTOS-specific symbols. Proxy and target deals only with addresses. Client
can resolve symbols to it addresses.

Hook `qSymbol::` request and request client for all needed symbols (by sending responses directly to
client with needed symbol names).

After that bypass initial packet to the target.

## Threads update

Update threads counts and information at least:

1. On Break packet... (UPD: no, just on reply to Break packet)
2. Some commands like "continue", "step" and so on may affect threads.
   See "E.3 Stop Reply Packets" of GDB manual. At least: `c` (continue), `s` (step), `vCont`
   * Ref: https://www.sourceware.org/gdb/onlinedocs/gdb.html#Stop-Reply-Packets
   ```
   The ‘C’, ‘c’, ‘S’, ‘s’, ‘vCont’, ‘vAttach’, ‘vRun’, ‘vStopped’, and ‘?’ packets can receive any
   of the below as a reply. Except for ‘?’ and ‘vStopped’, that reply is only returned when the
   target halts. In the below the exact meaning of signal number is defined by the header
   include/gdb/signals.h in the GDB source code.

   In non-stop mode, the server will simply reply ‘OK’ to commands such as ‘vCont’; any stop will be
   the subject of a future notification. See Remote Non-Stop.
   ```
   
At least `T` response can be affected by threading: current threadid should be added (from OpenOCD):
```c++
snprintf(current_thread, sizeof(current_thread), "thread:%016" PRIx64 ";", target->rtos->current_thread);
...
snprintf(sig_reply, sizeof(sig_reply), "T%2.2x%s%s",
                               signal_var, stop_reason, current_thread);
```



## Threads registers request

Aka stacking inmplementation.

TBD

## Threads packets

1. `T`
    ```
    ‘T thread-id’
    
        Find out if the thread thread-id is alive. See thread-id syntax.
    
        Reply:
    
        ‘OK’
    
            thread is still alive
        ‘E NN’
    
            thread is dead
    ```
2. `H`
    ```
    ‘H op thread-id’
    
        Set thread for subsequent operations (‘m’, ‘M’, ‘g’, ‘G’, et.al.). Depending on the operation to be performed, op should be ‘c’ for step and continue operations (note that this is deprecated, supporting the ‘vCont’ command is a better option), and ‘g’ for other operations. The thread designator thread-id has the format and interpretation described in thread-id syntax.
    
        Reply:
    
        ‘OK’
    
            for success
        ‘E NN’
    
            for an error
    ```
3. `q` and `Q` (can be processes in generic way, like: `qSymbol` and "qXfer:threads:read:" should be
   processed)
   ```
   ‘q name params…’
   ‘Q name params…’
    
        General query (‘q’) and set (‘Q’). These packets are described fully in General Query Packets.
   ```
4. `c` and `s`
    ```
    ‘s [addr]’
    
        Single step, resuming at addr. If addr is omitted, resume at same address.
    
        This packet is deprecated for multi-threading support. See vCont packet.
    
        Reply: See Stop Reply Packets, for the reply specifications.
    
    ‘c [addr]’
    
        Continue at addr, which is the address to resume. If addr is omitted, resume at current address.
    
        This packet is deprecated for multi-threading support. See vCont packet.
    
        Reply: See Stop Reply Packets, for the reply specifications.
    ```
5. `R`
    ```
    ‘R XX’
    
        Restart the program being debugged. The XX, while needed, is ignored. This packet is only available in extended mode (see extended mode).
    
        The ‘R’ packet has no reply.
    ```
6. `gdb_get_registers_packet` / `rtos_get_gdb_reg_list` / `g` - get thread related registers list
7. `gdb_generate_thread_list` / `gdb_get_thread_list_chunk` / `qXfer:threads:read:`
