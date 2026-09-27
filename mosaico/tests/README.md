# 本地行为测试
在应用目录执行（macOS / Clang；仅模拟逻辑，无设备显示后端）：

```sh
clang++ -std=c++20 -fsanitize=address,undefined -I src tests/interaction_test.cpp -o /tmp/liquid_interaction_test
/tmp/liquid_interaction_test
clang++ -std=c++20 -O2 -Wl,-dead_strip -fsanitize=address,undefined -I "$MICROPIXEL_SDK_DIR/guest" tests/dynamics_test.cpp src/flip.cpp -o /tmp/liquid_dynamics_test
/tmp/liquid_dynamics_test
```
