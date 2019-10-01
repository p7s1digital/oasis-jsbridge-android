PATH=/usr/local/Cellar/llvm/8.0.1/bin:$PATH
llvm-profdata merge -output=code.profdata *.profraw
llvm-cov show ./runUnitTests -format=html -instr-profile=code.profdata ../../JniLocalRef.h -path-equivalence -use-color
