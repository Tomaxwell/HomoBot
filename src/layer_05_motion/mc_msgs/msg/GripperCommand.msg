# 夹爪控制命令

string side                    # "left" / "right"
uint8 command                  # 0=打开, 1=闭合, 2=指定位置
float64 position               # 目标开口宽度 [m]（command=2 时）
float64 max_effort             # 最大夹持力 [N]
builtin_interfaces/Duration timeout # 超时时间
builtin_interfaces/Time stamp
