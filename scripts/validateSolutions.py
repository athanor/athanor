#!/usr/bin/env python3
import sys
from itertools import chain
import fileinput
import os
import subprocess
def error(message):
    print("Error: " + sys.argv[0] + ": " + message, file=sys.stderr)
    sys.exit(1)


def verifyPaths():
    global solutionDir
    global pathToConjure
    global essenceFile
    global paramFile
    if not os.path.isdir(solutionDir):
        error(solutionDir + " does not point to an existing directory.")
    
    if not os.path.exists(essenceFile):
        error(essenceFile + " does not point to an conjure essence file.")
    if paramFile != None and not os.path.exists(paramFile):
        error(paramFile + " does not point to a conjure essence parameter file.")



def readSolution(file,solutionNumber):
    global solutionDir
    solutionFilename = solutionDir + "/solution" + str(solutionNumber) + ".solution";
    print("Reading solution into " + solutionFilename)
    with open(solutionFilename, "w") as outFile:
        for line in file:
            if "solution end" in line:
                return solutionFilename
            print(line,file=outFile)
        error("solution " + solutionFilename + " was not read correctly, did not find finishing line \"solution end\"")


def validateSolution(solutionFilename):
    print("Validating solution at " + solutionFilename)
    global pathToConjure
    global essenceFile
    global paramFile
    command = [pathToConjure, "validate-solution", "--essence", essenceFile]
    if paramFile != None:
        command += ["--param", paramFile]
    command += ["--solution", solutionFilename]
    try:
        subprocess.check_output(command, stderr=subprocess.STDOUT)
        print("solution valid")
    except subprocess.CalledProcessError:
        print("***Found invalid solution: " + solutionFilename + "\nValidate Command:\n " + '"' + '" "'.join(command) + '"')
        sys.exit(1)




def startReading():
    global numberHeadTailSolutions
    print("Waiting for solutions\n")
    numberSolutions = 0
    file = sys.stdin
    for line in file:
        if "solution start" in line:
            numberSolutions += 1
            solutionFilename = readSolution(file,numberSolutions)
            if numberHeadTailSolutions <= 0:
                validateSolution(solutionFilename)
    
    if numberSolutions == 0:
        print("***No solutions found.", file=sys.stderr)
        sys.exit(1)
    
    
    if numberHeadTailSolutions > 0:
        n=numberHeadTailSolutions
        solutionRange = (range(1,numberSolutions+1) if numberSolutions <= n
            else chain(range(1,int(n/2)+1), 
                range((numberSolutions+1)-int(n/2),numberSolutions+1)))
        for solutionNumber in solutionRange:
            solutionFilename = solutionDir + "/solution" + str(solutionNumber) + ".solution";
            validateSolution(solutionFilename)
    
    print("Success, all solutions valid.\nNumber solutions found: " + str(numberSolutions))




if __name__ == "__main__":
    try:
        numberHeadTailSolutions=int(sys.argv[1])
        del sys.argv[1]
    except ValueError:
        numberHeadTailSolutions=0
        
    try:
        solutionDir = sys.argv[1]
        pathToConjure = sys.argv[2]
        essenceFile = sys.argv[3]
        if len(sys.argv) == 5:
            paramFile = sys.argv[4]
        else:
            paramFile = None
    except IndexError:
        error("""usage:\n" + sys.argv[0] + " [num_head_tail_sols] solution_dest_directory conjure_exec_command essence_file [essence_param_file]
num_head_tail_sols is should be a number, if not specified each and every solution is verified before reading the next.  If num_head_tail_sols is specified as an int > 0, all solutions are first read, and then the first and last n/2 are vverified where n is num_head_tail_sols.
""")

    verifyPaths()
    startReading()
