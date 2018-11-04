%thread(0) {
wsproto_in -r ../npu2_bench/packets.cut*.wsproto | addlabelmember COUNT -c 0 -> $card0
}
%thread(1) {
$card0 | vectormatchnpu CONTENT -D /dev/lrl_npu0 -F ../bench_regexes.list  -L NPU_MATCH -M -P -m1 -v4 -> $card1
}
%thread(2) {
$card1 | vectormatchnpu CONTENT -D /dev/lrl_npu1 -F ../bench_regexes.list  -L NPU_MATCH -M -P -m1 -v4 -> $card2
}
%thread(3) {
$card2 | vectormatchnpu CONTENT -D /dev/lrl_npu2 -F ../bench_regexes.list  -L NPU_MATCH -M -P -m1 -v4 -> $npu_out
}
%thread(4) {
$npu_out | labelstat | print -V
}
