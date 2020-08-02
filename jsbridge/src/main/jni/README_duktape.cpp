To add/update duktape (tested with duktape v2.2.0):
$ git clone https://github.com/svaarala/duktape
$ cd duktape
$ python tools/configure.py --source-directory src-input --output-directory tmp-output \
  -DDUK_USE_SYMBOL_BUILTIN \
  -DDUK_USE_DEBUGGER_SUPPORT -DDUK_USE_INTERRUPT_COUNTER -DDUK_USE_DEBUGGER_INSPECT \
  -DDUK_USE_DEBUGGER_PAUSE_UNCAUGHT -DDUK_USE_DEBUGGER_FWD_LOGGING \
  -DDUK_USE_DEBUGGER_FWD_PRINTALERT -DDUK_USE_DEBUGGER_THROW_NOTIFY \
  -DDUK_USE_CPP_EXCEPTIONS
- copy duktape.h, duktape.c, duk_config.c from tmp-output into jsbridge/src/duktape/cpp/duktape
- rename duktape.c into duktape.cpp
- manually fix issue in duktape.cpp which causes an issue on Android API 24 devices:
  replace DUK_SPRINTF call in duk_bi_symbol_constructor_shared() into DUK_SNPRINTF as follows:

  DUK_INTERNAL duk_ret_t duk_bi_symbol_constructor_shared(duk_hthread *thr) {
     ...
     // CRASHES ON API 24:
     //p += DUK_SPRINTF((char *) p, "\xFF" "%lx-%lx",
		 //                 (unsigned long) thr->heap->sym_counter[1],
		 //                 (unsigned long) thr->heap->sym_counter[0]);

     // FIX FOR API 24:
     p += DUK_SNPRINTF((char *) p, 18, "\xFF" "%lx-%lx",
		                  (unsigned long) thr->heap->sym_counter[1],
		                  (unsigned long) thr->heap->sym_counter[0]);
     ...
  }



Note 1: duk_console is based on the version in duktape/extras/console but adjusted to use Timber via JNI
Note 2: promise.js is based on the version in duktape/polyfills but adjusted to properly handle errors
(see https://github.com/svaarala/duktape/issues/1934)
Note 3: duk_trans_socket.h and duk_trans_socket_unix.c are based on the version in duktape/examples/debug-trans-socket
and may need to be adjusted in the future

