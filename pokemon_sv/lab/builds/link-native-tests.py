# Build orchestration only: the registered tests and their runner are native C++.
import json,shlex,subprocess
from pathlib import Path
root=Path.cwd();build=root/'build-prep/app'
commands=json.loads((build/'compile_commands.json').read_text())
c=next(x for x in commands if x['file'].endswith('/VideoGames/PokemonSV/PokemonSV_Tests.cpp'))
args=shlex.split(c['command']);args[args.index('-o')+1]=str(root/'build-prep/bbq-native-runner.o');args[-1]=str(root/'build-prep/bbq-native-runner.cpp')
subprocess.run(args,cwd=c['directory'],check=True)
commands=subprocess.check_output(['ninja','-t','commands','SerialPrograms.app/Contents/MacOS/SerialPrograms'],cwd=build,text=True).splitlines()
link=next(x for x in commands if ' -o SerialPrograms.app/Contents/MacOS/SerialPrograms ' in x)
args=shlex.split(link)
while args[0] in (':','&&'):args.pop(0)
if '&&' in args:args=args[:args.index('&&')]
args=[str(root/'build-prep/bbq-native-runner.o') if x.endswith('/Source/CommonFramework/Main.cpp.o') else x for x in args]
i=args.index('AGL') if 'AGL' in args else -1
if i>0 and args[i-1]=='-framework':args[i-1:i+1]=['/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk/System/Library/Frameworks/AGL.framework/AGL.tbd']
args[args.index('-o')+1]=str(build/'BBQNativeTests')
subprocess.run(args,cwd=build,check=True,stdout=subprocess.DEVNULL)
print(build/'BBQNativeTests')
