%thread(0) {
wsproto_in -r ../npu2_bench/packets.cut*.wsproto -> $data_in
$data_in | vectormatchnpu CONTENT -D /dev/lrl_npu0 -F ../npu2_bench_set.npup -L NPU_MATCH -m3 -M -v5
}
