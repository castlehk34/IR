#!/bin/bash

# PID별 최대 CPU 사용률을 저장할 임시 파일
TEMP_FILE="/tmp/pid_max_cpu.txt"

# 현재 디렉토리
TARGET_DIR=$(pwd)

# 초기화
> "$TEMP_FILE"

while true; do
    # 현재 CPU 사용률과 명령어 가져오기 (현재 디렉토리에서 실행된 프로세스만)
    ps aux | awk -v target="$TARGET_DIR" 'NR>1 {print $2, $3, $11}' | while read pid cpu cmd; do
        # 프로세스의 작업 디렉토리 확인
        proc_dir=$(pwdx $pid 2>/dev/null | awk '{print $2}')
        
        if [ "$proc_dir" == "$TARGET_DIR" ]; then
            # 이전 최대값 불러오기
            prev_max=$(grep -w "^$pid " "$TEMP_FILE" | awk '{print $2}')
            
            # 최대값 갱신
            if [ -z "$prev_max" ] || (( $(echo "$cpu > $prev_max" | bc -l) )); then
                # 기존 PID 삭제 후 새로 기록
                sed -i "/^$pid /d" "$TEMP_FILE"
                echo "$pid $cpu $cmd" >> "$TEMP_FILE"
            fi
        fi
    done

    # 화면 출력
    clear
    echo "Maximum CPU usage of running processes in the current directory: "
    echo "---------------------------------------------------------"
    cat "$TEMP_FILE" | sort -k2 -nr | awk '{printf "PID: %s, Max CPU: %.2f%%, CMD: %s\n", $1, $2, $3}'
    echo "---------------------------------------------------------"
    sleep 0.1
done

