#!/usr/bin/env python3
import sys
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
    print("Waiting for solutions\n")
    numberSolutions = 0
    file = sys.stdin
    for line in file:
        if "solution start" in line:
            numberSolutions += 1
            solutionFilename = readSolution(file,numberSolutions)
            validateSolution(solutionFilename)
    if numberSolutions == 0:
        print("***No solutions found.", file=sys.stderr)
        sys.exit(1)
    else:
        print("Success, all solutions valid.\nNumber solutions found: " + str(numberSolutions))


    
if __name__ == "__main__":
    try:
        solutionDir = sys.argv[1]
        pathToConjure = sys.argv[2]
        essenceFile = sys.argv[3]
        if len(sys.argv) == 5:
            paramFile = sys.argv[4]
        else:
            paramFile = None
    except IndexError:
        error("usage:\n" + sys.argv[0] + " solution_dest_directory conjure_exec_command essence_file [essence_param_file]")
    verifyPaths()
    startReading()
