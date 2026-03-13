from langchain_core.tools import tool
from langchain_core.tools import StructuredTool
from typing import Optional, Type, TypedDict, ClassVar
from pydantic import BaseModel, Field
from langchain_core.callbacks import (
    AsyncCallbackManagerForToolRun,
    CallbackManagerForToolRun,
)
from langchain_openai import ChatOpenAI
from langchain_core.messages import HumanMessage, SystemMessage
from langchain_core.output_parsers import StrOutputParser
from langchain_core.prompts import ChatPromptTemplate
from langchain_core.tools import BaseTool
from langgraph.graph import StateGraph, START, END
from langchain_openrouter import ChatOpenRouter
from dotenv import load_dotenv
import subprocess
from pathlib import Path
from datetime import datetime
from prompts.hw_sw_partition_prompt import SYSTEM_PROMPT, CODE_ANALYSIS_PROMPT

""" enviroment set up """
load_dotenv()
benchmark_path = "benchmark"


def generate_gprof_report(app, output_file='gprof.txt'):
    compile_command = f"g++ -pg -o {benchmark_path}/{app}/{app} {benchmark_path}/{app}/{app}.cpp"
    subprocess.run(compile_command, shell=True, check=True)

    run_command = f"./{benchmark_path}/{app}/{app}"
    subprocess.run(run_command, shell=True, check=True)

    gprof_command = f"gprof {benchmark_path}/{app}/{app} gmon.out > {benchmark_path}/{app}/{output_file}"
    subprocess.run(gprof_command, shell=True, check=True)

    report_path = benchmark_path + '/' + app + '/' + output_file
    with open(report_path, 'r') as file:
        report = file.read()

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    profiling_dir = Path("profiling_report") / app
    profiling_dir.mkdir(parents=True, exist_ok=True)
    profiling_path = profiling_dir / f"{app}_{timestamp}.cpp"
    profiling_path.write_text(report, encoding="utf-8")

    return report


def analysis_report(app, report_content):
    PROMPT_COMPLETE = ""
    PROMPT_COMPLETE += SYSTEM_PROMPT.replace("{ALGO_NAME}", app)

    app_file_path = benchmark_path + '/' + app + '/' + app + '.cpp'
    with open(app_file_path, 'r') as app_file:
        code_content = app_file.read()

    _CODE_ANALYSIS_PROMPT = CODE_ANALYSIS_PROMPT.replace("{CODE_CONTENT}", code_content)
    _CODE_ANALYSIS_PROMPT = _CODE_ANALYSIS_PROMPT.replace("{REPORT_CONTENT}", report_content)

    PROMPT_COMPLETE += _CODE_ANALYSIS_PROMPT
    print(PROMPT_COMPLETE)

    #chat_model = ChatOpenAI(model='gpt-4o')
    chat_model = ChatOpenRouter(model='stepfun/step-3.5-flash:free',temperature=0)
    messages = [
        HumanMessage(content=PROMPT_COMPLETE),
    ]
    response = chat_model.invoke(messages)
    print(response)

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    bottleneck_dir = Path("bottleneck_report") / app
    bottleneck_dir.mkdir(parents=True, exist_ok=True)
    bottleneck_path = bottleneck_dir / f"{app}_{timestamp}.txt"
    bottleneck_path.write_text(str(response), encoding="utf-8")

    return str(response)


class AutoAnalysisInput(BaseModel):
    application: str = Field(description="application name")


class AutoAnalysisTool(BaseTool):
    name: ClassVar[str] = "Auto Code Analysis Tool"
    description: ClassVar[str] = "Used to call the gprof tool to generate performance reports, analyze the reports using LLM, and output various performance indicators"
    args_schema: ClassVar[Type[BaseModel]] = AutoAnalysisInput
    return_direct: ClassVar[bool] = True

    def _run(
        self, application: str, run_manager: Optional[CallbackManagerForToolRun] = None
    ) -> str:
        report_content = generate_gprof_report(application)
        analysis_report(application, report_content)


class GraphState(TypedDict):
    application: str
    report_content: str
    analysis_result: str


def generate_report_node(state: GraphState) -> GraphState:
    report_content = generate_gprof_report(state["application"])
    return {**state, "report_content": report_content}


def analysis_node(state: GraphState) -> GraphState:
    analysis_result = analysis_report(state["application"], state["report_content"])
    return {**state, "analysis_result": analysis_result}


graph = StateGraph(GraphState)
graph.add_node("generate_report", generate_report_node)
graph.add_node("analysis", analysis_node)
graph.add_edge(START, "generate_report")
graph.add_edge("generate_report", "analysis")
graph.add_edge("analysis", END)
app = graph.compile()


if __name__ == "__main__":
    mytool = AutoAnalysisTool()
    print(mytool.name)
    print(mytool.description)
    print(mytool.args)
    print(mytool.return_direct)

    inputs: GraphState = {
        "application": "fir",
        "report_content": "",
        "analysis_result": "",
    }
    result = app.invoke(inputs)
    print(result["analysis_result"])
