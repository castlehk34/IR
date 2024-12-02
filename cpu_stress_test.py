import multiprocessing
import os

def cpu_stress():
    print(f"Starting process {os.getpid()}")
    while True:
        result = sum(i * i for i in range(10**6))

if __name__ == "__main__":
    num_processes = multiprocessing.cpu_count()  # 모든 CPU 코어 사용
    processes = []
    for _ in range(num_processes):
        process = multiprocessing.Process(target=cpu_stress)
        processes.append(process)
        process.start()

    for process in processes:
        process.join()

