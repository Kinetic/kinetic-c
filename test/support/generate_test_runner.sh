#!/bin/sh
grep -n -o -e "^void test_[a-zA-Z0-9_]\+(void)" $1 |
env testfile=$1 awk '
{
    ln=$1
    gsub(":.*","",ln)
    test_name = $2
    sub("\\(.*","",test_name)
    tests[test_name] = ln
}
END {
    printf("//======= TEST SUITE RUNNER: %s =====\n", ENVIRON["testfile"])
    print("#include \"unity.h\"")
    for (k in tests) {
        printf("void %s(void);\n", k)
    }
    print("");
    print("int main(void)")
    print("{")
    printf("    UnityBegin(\"%s\");\n", ENVIRON["testfile"])
    for (k in tests) {
        printf("    RUN_TEST(%s, %d);\n", k, tests[k])
    }
    print "    return (UnityEnd());"
    print "}\n"
}
'
