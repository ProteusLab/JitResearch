import subprocess

sim_exe_path = None


class BenchmarkData:
    def __init__(self, time_s: float, instruction_count: int, threshold_num: int):
        self.time_s = time_s
        self.instruction_count = instruction_count
        self.threshold_num = threshold_num

    def mips(self) -> float:
        return self.instruction_count / (1000000 * self.time_s)

    def time(self) -> float:
        return self.time_s

    def icount(self) -> int:
        return self.instruction_count

    def threshold(self) -> int:
        return self.threshold_num


class BenchmarkRequest:
    def __init__(self, elf_path: str, threshold_num: int = None, jit_name: str = 'interp'):
        self.threshold_num = threshold_num
        self.jit_name = jit_name
        self.elf_path = elf_path
    

class BenchmarkExitResult:
    def __init__(self, exit_code: int, stdout: str, stderr: str):
        self.exit_code = exit_code
        self.stdout = stdout.rstrip()
        self.stderr = stderr.rstrip()

    def benchmark_data(self) -> BenchmarkData:
        self.assert_success()
        return self.parse_stdout()
    
    def assert_success(self) -> None:
        assert self.exit_code == 0, self.stderr
        assert self.stderr == ''

    def assert_error(self) -> None:
        assert self.exit_code != 0, self.stdout
        assert self.stdout == ''

    def parse_stdout(self) -> BenchmarkData:
        result = {}
        for line in self.stdout.splitlines():
            splitted = line.split(':', 1)
            if (len(splitted) == 1): 
                continue
            key = splitted[0].strip()
            value = splitted[1].strip().rstrip('s')
            result[key] = value
        return BenchmarkData(
            float(result["time"]), 
            int(result["icount"]), 
            int(result["threshold"])
        )


class Benchmark:
    def __init__(self, request: BenchmarkRequest):
        self.request = request
        pass
    
    def run(self):
        command = [str(sim_exe_path)]
        if (self.request.threshold_num != None):
            # command.extend(['--thsreshold', str(self.request.threshold_num)])
            command.extend(['--jit', str(self.request.jit_name)])
        command.extend([str(self.request.elf_path)])
        self._process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        
        assert self._process.stdin is not None, "Failed to open STDIN"
        assert self._process.stdout is not None, "Failed to open STDOUT"
        assert self._process.stderr is not None, "Failed to open STDERR"

    def exit(self, timeout=100) -> BenchmarkExitResult:
        self._process.stdin.close()
        return self.wait_for_exit(timeout)

    def wait_for_exit(self, timeout=10) -> BenchmarkExitResult:
        exit_code = self._process.wait(timeout)
        stdout = self._process.stdout.read()
        stderr = self._process.stderr.read()
        return BenchmarkExitResult(exit_code, stdout, stderr)
