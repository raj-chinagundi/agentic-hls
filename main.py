from typing import TypedDict
from pathlib import Path
from datetime import datetime
import importlib.util
import shutil
import subprocess

from langchain_openai import ChatOpenAI
from langchain_core.messages import HumanMessage
from langgraph.graph import StateGraph, START, END


profiling_workspace_root = Path(__file__).resolve().parent.parent
profiling_default_benchmark_root = profiling_workspace_root / "source_code" / "HLSPilot" / "benchmark"
profiling_default_report_dir = profiling_workspace_root / "profiling_report"
bottleneck_default_report_dir = profiling_workspace_root / "bottleneck_report"
profiling_prompt_file = (
	profiling_workspace_root / "source_code" / "HLSPilot" / "src" / "hw_sw_partition" / "hw_sw_partition_prompt.py"
)


class profiling_agent_state(TypedDict):
	subfolder: str
	program_name: str
	benchmark_root: str
	profiling_output_dir: str
	bottleneck_output_dir: str
	extra_compile_flags: str
	profiling_timestamp: str
	source_file_path: str
	profiling_report_path: str
	bottleneck_report_path: str
	bottleneck_report_text: str
	status: str
	error: str


def profiling_resolve_source_file(subfolder: str, program_name: str, benchmark_root: Path) -> Path:
	app_dir = benchmark_root / subfolder
	source_file = app_dir / f"{program_name}.cpp"
	if not source_file.exists():
		raise FileNotFoundError(
			f"Expected source file not found: {source_file}. Expected layout is benchmark/<subfolder>/<program_name>.cpp"
		)
	return source_file


def profiling_generate_report_node(state: profiling_agent_state) -> profiling_agent_state:
	try:
		resolved_benchmark_root = (
			Path(state.get("benchmark_root", "")).expanduser()
			if state.get("benchmark_root", "")
			else profiling_default_benchmark_root
		)
		subfolder = state["subfolder"]
		program_name = state["program_name"]
		extra_compile_flags = state.get("extra_compile_flags", "-O2 -std=c++17")

		source_file = profiling_resolve_source_file(subfolder, program_name, resolved_benchmark_root)

		profiling_build_dir = profiling_workspace_root / ".build_profiles" / subfolder
		profiling_build_dir.mkdir(parents=True, exist_ok=True)
		profiling_executable = profiling_build_dir / program_name

		compile_command = [
			"g++",
			"-pg",
			*extra_compile_flags.split(),
			"-o",
			str(profiling_executable),
			str(source_file),
		]
		subprocess.run(compile_command, capture_output=True, text=True, check=True)

		subprocess.run(
			[str(profiling_executable)],
			cwd=str(source_file.parent),
			capture_output=True,
			text=True,
			check=True,
		)

		gmon_out = source_file.parent / "gmon.out"
		if not gmon_out.exists():
			raise FileNotFoundError(
				f"Expected gprof data file gmon.out was not created in {source_file.parent}"
			)

		if not shutil.which("gprof"):
			raise EnvironmentError(
				"gprof is not installed or not available in PATH. Install gprof/binutils and retry."
			)

		gprof_result = subprocess.run(
			["gprof", str(profiling_executable), str(gmon_out)],
			capture_output=True,
			text=True,
			check=True,
		)
		gprof_report = gprof_result.stdout

		profiling_base_report_dir = (
			Path(state.get("profiling_output_dir", "")).expanduser()
			if state.get("profiling_output_dir", "")
			else profiling_default_report_dir
		)
		profiling_report_dir = profiling_base_report_dir / subfolder
		profiling_report_dir.mkdir(parents=True, exist_ok=True)
		profiling_timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

		profiling_report_path = profiling_report_dir / f"{program_name}_{profiling_timestamp}.cpp"
		profiling_report_path.write_text(gprof_report, encoding="utf-8")

		return {
			**state,
			"profiling_timestamp": profiling_timestamp,
			"source_file_path": str(source_file),
			"profiling_report_path": str(profiling_report_path),
			"status": "profiling_success",
			"error": "",
		}
	except Exception as exc:
		return {
			**state,
			"status": "profiling_failed",
			"error": str(exc),
		}


def profiling_load_prompt_module():
	if not profiling_prompt_file.exists():
		raise FileNotFoundError(f"Prompt file not found: {profiling_prompt_file}")

	spec = importlib.util.spec_from_file_location("hw_sw_partition_prompt", str(profiling_prompt_file))
	if spec is None or spec.loader is None:
		raise RuntimeError(f"Unable to load prompt module from: {profiling_prompt_file}")

	module = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(module)
	return module


def profiling_generate_bottleneck_report_node(state: profiling_agent_state) -> profiling_agent_state:
	try:
		if state.get("status") != "profiling_success":
			return state

		prompt_module = profiling_load_prompt_module()
		system_prompt = prompt_module.SYSTEM_PROMPT
		code_analysis_prompt = prompt_module.CODE_ANALYSIS_PROMPT

		source_file = Path(state["source_file_path"])
		profiling_report_path = Path(state["profiling_report_path"])

		source_content = source_file.read_text(encoding="utf-8")
		report_content = profiling_report_path.read_text(encoding="utf-8")

		prompt_complete = system_prompt.replace("{ALGO_NAME}", state["program_name"])
		prompt_complete += code_analysis_prompt.replace("{CODE_CONTENT}", source_content).replace(
			"{REPORT_CONTENT}", report_content
		)

		chat_model = ChatOpenAI(model="gpt-4o", temperature=0)
		response = chat_model.invoke([HumanMessage(content=prompt_complete)])
		bottleneck_text = response.content if isinstance(response.content, str) else str(response.content)

		bottleneck_base_dir = (
			Path(state.get("bottleneck_output_dir", "")).expanduser()
			if state.get("bottleneck_output_dir", "")
			else bottleneck_default_report_dir
		)
		bottleneck_subfolder_dir = bottleneck_base_dir / state["subfolder"]
		bottleneck_subfolder_dir.mkdir(parents=True, exist_ok=True)

		bottleneck_report_path = (
			bottleneck_subfolder_dir / f"{state['program_name']}_{state['profiling_timestamp']}.txt"
		)
		bottleneck_report_path.write_text(bottleneck_text, encoding="utf-8")

		return {
			**state,
			"bottleneck_report_path": str(bottleneck_report_path),
			"bottleneck_report_text": bottleneck_text,
			"status": "bottleneck_success",
		}
	except Exception as exc:
		fallback_text = f"Bottleneck report generation failed: {exc}"
		bottleneck_base_dir = (
			Path(state.get("bottleneck_output_dir", "")).expanduser()
			if state.get("bottleneck_output_dir", "")
			else bottleneck_default_report_dir
		)
		bottleneck_subfolder_dir = bottleneck_base_dir / state.get("subfolder", "unknown")
		bottleneck_subfolder_dir.mkdir(parents=True, exist_ok=True)
		timestamp = state.get("profiling_timestamp") or datetime.now().strftime("%Y%m%d_%H%M%S")
		program_name = state.get("program_name", "unknown")
		bottleneck_report_path = bottleneck_subfolder_dir / f"{program_name}_{timestamp}.txt"
		bottleneck_report_path.write_text(fallback_text, encoding="utf-8")
		return {
			**state,
			"bottleneck_report_path": str(bottleneck_report_path),
			"bottleneck_report_text": fallback_text,
			"status": "bottleneck_failed",
			"error": str(exc),
		}


graph = StateGraph(profiling_agent_state)
graph.add_node("profiling_generate_report_node", profiling_generate_report_node)
graph.add_node("profiling_generate_bottleneck_report_node", profiling_generate_bottleneck_report_node)
graph.add_edge(START, "profiling_generate_report_node")
graph.add_edge("profiling_generate_report_node", "profiling_generate_bottleneck_report_node")
graph.add_edge("profiling_generate_bottleneck_report_node", END)
app = graph.compile()


if __name__ == "__main__":
	inputs: profiling_agent_state = {
		"subfolder": "bfs",
		"program_name": "bfs",
		"benchmark_root": "",
		"profiling_output_dir": "",
		"bottleneck_output_dir": "",
		"extra_compile_flags": "-O2 -std=c++17",
		"profiling_timestamp": "",
		"source_file_path": "",
		"profiling_report_path": "",
		"bottleneck_report_path": "",
		"bottleneck_report_text": "",
		"status": "",
		"error": "",
	}

	result = app.invoke(inputs)
	print(f"Status: {result['status']}")
	print(f"Profiling report: {result.get('profiling_report_path', '')}")
	print(f"Bottleneck report: {result.get('bottleneck_report_path', '')}")
	if result.get("error"):
		print(f"Error: {result['error']}")
