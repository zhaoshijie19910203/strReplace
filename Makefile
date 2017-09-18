logAnalysis: logAnalysis.c
	g++ -o logAnalysis logAnalysis.c 

clean:
	rm logAnalysis 
	rm *_result*.*
	ls -F | grep / | xargs rm -rf

cleanProgram:
	rm logAnalysis

cleanFile:
	rm *_result*.*
	ls -F | grep / | xargs rm -rf	
