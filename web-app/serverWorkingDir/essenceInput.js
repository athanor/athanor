
window.onload = function() {
    document.getElementById("input_file").addEventListener('change',
            handleFileSelect, false);
}



function display(message, escape) {
    var pre = document.getElementById("console_output")
    pre.innerHTML = ""
    if (escape) {
        var text = document.createTextNode(message);
        pre.appendChild(text);
    } else {
        pre.innerHTML = message
    }
}

function displayError(message, escape) {
    var pre = document.getElementById("console_output")
    pre.innerHTML = ""
    if (escape) {
        var text = document.createTextNode("Error: " + message);
        pre.appendChild(text);
    } else {
        pre.innerHTML = "Error: " + message
    }
    document.getElementById("run_button").innerHTML = "RUN";
}

function handleFileSelect(event) {
    try {
        var file = event.target.files[0];
        document.getElementById("input_file").value = "";
        var reader = new FileReader();
        reader.onload = processFile;
        reader.readAsText(file);
    } catch(ex) {
        alert("Unexpected error: " + ex + "\n\nPress okay to restart");
        location.reload()
    }
}

function processFile(file) {
    if (document.getElementById("nav-spec-tab").getAttribute("aria-selected") === "true") {
        document.getElementById("spec_text").value = file.target.result
    } else {
        document.getElementById("param_text").value = file.target.result
    }
}


function runButtonPressed() {
    var button = document.getElementById("run_button")
    if (button.innerHTML === "RUN") {
        button.innerHTML = "STOP";
        parseAndRun()
    } else {
        button.innerHTML = "RUN";
        display("cancelling...");
        if (window.athanorWorker) {
            window.athanorWorker.terminate()
            window.athanorWorker = null;
            display("cancelled");
        }
    }
}

/* submit essence spec and param to server and return JSON representation. */
function parseAndRun() {
    showStats({"solutionNumber":"--",
    "wallTime":"--",
    "numberIterations":"--",
    "violation":"--",
    "hasObjective":true,
    "objective":"--"})
    display("Uploading files to server for translation...", true);
    data = {}
    data["spec"] = document.getElementById("spec_text").value
    data["param"] = document.getElementById("param_text").value
    var xhr = new XMLHttpRequest();
    xhr.open("POST","essenceToJson",true);
    xhr.setRequestHeader('Content-Type', 'application/json; charset=UTF-8');
    xhr.onload = function() {
        handleServerResponse(xhr)
    }
    // send the collected data as JSON
    xhr.send(JSON.stringify(data));
}

function handleServerResponse(xhr) {
    var button = document.getElementById("run_button");
    if (button.innerHTML === "RUN") {
        //button was pressed again, switching it from STOP to RUN
        display("cancelled");
        return
    }
    if (xhr.status === 200) {
        jsonResponse = JSON.parse(xhr.responseText)
        if (jsonResponse["spec"].status != "success") {
            displayError(jsonResponse["spec"]["data"],true)
            return
        }
        if (jsonResponse["param"].status != "success") {
            displayError(jsonResponse["param"]["data"],true)
            return
        }
        display("Received translation, initialising solver...");
        runAthanor(jsonResponse["spec"]["data"], jsonResponse["param"]["data"])
    } else {
        displayError(xhr.responseText,false)
    }
}

function showStats(stats) {
    document.getElementById("stats_solution_number").innerHTML = stats.solutionNumber;
    document.getElementById("stats_wall_time").innerHTML = stats.wallTime;
    document.getElementById("stats_number_iterations").innerHTML = stats.numberIterations;
    document.getElementById("stats_violation").innerHTML = stats.violation;
    if (stats.hasObjective) {
        document.getElementById("stats_objective").innerHTML = stats.objective;
    }
}

function runAthanor(spec,param) {
    if (window.athanorWorker) {
        window.athanorWorker.terminate()
    }
    window.athanorWorker = new Worker("athanorWorker.js");
    window.athanorWorker.onmessage = function(e) {
        window.myVar = e
        var pre = document.getElementById("console_output")
        if (e.data.display) {
            pre.innerHTML = e.data.display;
        }
        if (e.data.stats) {
            showStats(e.data.stats);
        }
    };
    
    window.athanorWorker.postMessage([spec,param])
}

