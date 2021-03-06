function printSolution(solution) {
    console.log("received solution ")
    postMessage({"display":solution})
}


function printStats(stats) {
    //must copy the stats as the original is a special kind of object
    statsCopy = {
        "solutionNumber": stats.solutionNumber,
        "wallTime": stats.wallTime,
        "numberIterations": stats.numberIterations,
        "violation":stats.violation,
        "hasObjective":stats.hasObjective,
        "objective":stats.objective
    }
    postMessage({"stats":statsCopy})
}

var Module = {}
onmessage = function(e) {
    Module = {
        onRuntimeInitialized : function() {
            if (e.data.initCall) {
                postMessage({"display":"Solver loaded!"})
                return
            }
            postMessage({"display":"Solver initialised, searching for solutions..."})
            Module.runSolverWasm(e.data[0],e.data[1]);
        }
    };
    self.importScripts("athanor.js");;
    
    
};

function reportError(message) {
    postMessage({"display":message,"escape":true,"endOfSearch":true})
}

function reportEndOfSearch() {
    postMessage({"endOfSearch":true})
}