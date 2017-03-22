%thread(1) {
wsproto_in -r ../npu2_bench/packets.cut*.wsproto -> $data_in
#pcap -r ../*.pcap  -r ../*.pcap -r ../*.pcap -> $data_in
#$data_in | workbalance -N 3 -J CARDS -L CARD -> $card
#$data_in | flush -N -t 1s -> $flush_var
}
%thread(2) {
$data_in | vectormatchnpu CONTENT -D /dev/lrl_npu0 -F ../npu2_bench_set.npup -L NPU_MATCH -m3 -M -v4 -> $npu_out
}
%thread(3) {
$data_in | vectormatchnpu CONTENT -D /dev/lrl_npu1 -F ../npu2_bench_set.npup -L NPU_MATCH -m3 -M -v4 -> $npu_out
}
%thread(4) {
$data_in | vectormatchnpu CONTENT -D /dev/lrl_npu2 -F ../npu2_bench_set.npup -L NPU_MATCH -m3 -M -v4 -> $npu_out
}
%thread(5) {
$data_in | vectormatchnpu CONTENT -D /dev/lrl_npu3 -F ../npu2_bench_set.npup -L NPU_MATCH -m3 -M -v4 -> $npu_out
}
%thread(6) {
$npu_out | bandwidth
}
