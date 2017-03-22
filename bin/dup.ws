%thread(1) {
wsproto_in -r '../npu2_bench/packets.cut*.wsproto' -> $data_in
$data_in | bundle -N 256 -> $card
$data_in | flush -N -t 1s -> $flush_var
}
%thread(2) {
$card | unbundle -> $card0
$flush_var, $card0 | vectormatchnpu CONTENT -D /dev/lrl_npu0 -F ../npu2_bench_set.npup -L NPU_MATCH_0 -m1 -M -v4 -> $npu_out0
}
%thread(3) {
$card | unbundle -> $card1
$flush_var, $card1 | vectormatchnpu CONTENT -D /dev/lrl_npu1 -F ../npu2_bench_set.npup -L NPU_MATCH_1 -m1 -M -v4 -> $npu_out1
}
%thread(4) {
$card|unbundle -> $card2
$flush_var, $card2 | vectormatchnpu CONTENT -D /dev/lrl_npu2 -F ../npu2_bench_set.npup -L NPU_MATCH_2 -m1 -M -v4 -> $npu_out2
}
%thread(5) {
$npu_out0,$npu_out1,$npu_out2 | labelstat
}
