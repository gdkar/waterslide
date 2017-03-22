%thread(1) {
wsproto_in -r '../npu2_benchc/packets.*.wsproto' -> $card0
$card0 | vectormatchnpu CONTENT -D /dev/lrl_npu0 -F ../npu2_bench_set.npup -L NPU_MATCH_0 -m3 -M -v4 -> $npu_out0
}
