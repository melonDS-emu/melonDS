from os import chdir,listdir,environ
from os.path import isfile,join,isdir
from subprocess import DEVNULL, run
import sys

ignored_files = "-ignore-filename-regex=glib -ignore-filename-regex=fuzz -ignore-filename-regex=helper -ignore-filename-regex=h$"

if __name__ == "__main__":
    chdir("build/fuzzing/out")
    available_targets = [exe for exe in listdir("../") if isfile(join("..", exe))]
    available_corpus_path = [exe for exe in listdir("../../../fuzzing/") if isdir(join("../../../fuzzing/", exe))]
    available_result_types = ["export", "show", "report"]
    if len(sys.argv) != 4 or sys.argv[1] not in available_targets or sys.argv[2] not in available_corpus_path or sys.argv[3] not in available_result_types:
        print("usage : python coverage.py fuzz_target IN_protol result_type")
        print(" - available targets : ")
        print(available_targets)
        print(" - available_corpus_path : ")
        print(available_corpus_path)
        print(" - available result types : ")
        print(available_result_types)
        exit(0)
    fuzzing_target = sys.argv[1]
    corpus_path = "../../../fuzzing/"+sys.argv[2]+"/"
    result_type = sys.argv[3]
    if fuzzing_target in available_targets:
        environ["LLVM_PROFILE_FILE"] = fuzzing_target + "_%p.profraw"
        corpus = listdir(corpus_path)
        for f in corpus:
            #print(corpus_path+f)
            run(["../" + fuzzing_target, corpus_path+f,"-detect_leaks=0"], stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL)
        run(["llvm-profdata merge -sparse " + fuzzing_target + "_*.profraw -o " + fuzzing_target + ".profdata"], shell=True)
        if result_type == "export" :
            run(["llvm-cov show ../" + fuzzing_target + " -format=html -output-dir=../report -instr-profile=" + fuzzing_target + ".profdata " + ignored_files], shell=True)
        elif result_type == "show" :
            run(["llvm-cov show ../" + fuzzing_target + " -instr-profile=" + fuzzing_target + ".profdata " + ignored_files], shell=True)
        else:
            run(["llvm-cov report ../" + fuzzing_target + " -instr-profile=" + fuzzing_target + ".profdata " + ignored_files], shell=True)
