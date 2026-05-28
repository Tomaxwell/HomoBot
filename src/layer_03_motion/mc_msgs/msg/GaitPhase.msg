# mc_msgs/msg/GaitPhase.msg
# 步态相位广播（MC → UC，供摆臂补偿）

builtin_interfaces/Time stamp
uint8 phase                  # 当前步态相位（同 LowerBodyStatus.gait_phase）
float32 progress             # 步态周期进度 0.0~1.0
bool left_contact            # 左足触地
bool right_contact           # 右足触地
float32 expected_duration    # 当前相位预期持续时间 [s]（供 PnC 预测触地时刻，优化路径规划中的步态同步）
