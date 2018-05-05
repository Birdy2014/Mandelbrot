var iterationen = 300;
var colors = ["#000000", "#55AA22", "#DD5599"];

function setValues() {
    window.sessionStorage.setItem("iterationen", iterationen.toString());
    window.sessionStorage.setItem("colors0", colors[0]);
    window.sessionStorage.setItem("colors1", colors[1]);
    window.sessionStorage.setItem("colors2", colors[2]);
}

function getValues() {
    if (window.sessionStorage.getItem("iterationen") !== null) {
        iterationen = parseInt(window.sessionStorage.getItem("iterationen"));
        colors[0] = window.sessionStorage.getItem("colors0");
        colors[1] = window.sessionStorage.getItem("colors1");
        colors[2] = window.sessionStorage.getItem("colors2");
    }
}

function load() {
    var colorPicker1 = document.getElementById("colorPicker1");
    var colorPicker2 = document.getElementById("colorPicker2");
    var colorPicker3 = document.getElementById("colorPicker3");
    var textFieldIterationen = document.getElementById("textFieldIterationen");

    getValues();

    colorPicker1.value = colors[0];
    colorPicker2.value = colors[1];
    colorPicker3.value = colors[2];
    textFieldIterationen.value = iterationen.toString();
}

function update() {
    var colorPicker1 = document.getElementById("colorPicker1");
    var colorPicker2 = document.getElementById("colorPicker2");
    var colorPicker3 = document.getElementById("colorPicker3");
    var textFieldIterationen = document.getElementById("textFieldIterationen");

    colors[0] = colorPicker1.value;
    colors[1] = colorPicker2.value;
    colors[2] = colorPicker3.value;
    iterationen = textFieldIterationen.value;

    setValues();
}

function resetSettings() {
    var colorPicker1 = document.getElementById("colorPicker1");
    var colorPicker2 = document.getElementById("colorPicker2");
    var colorPicker3 = document.getElementById("colorPicker3");
    var textFieldIterationen = document.getElementById("textFieldIterationen");

    textFieldIterationen.value = 300;
    colorPicker1.value = "#000000";
    colorPicker2.value = "#55AA22";
    colorPicker3.value = "#DD5599";

    update();
}