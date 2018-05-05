var xa;
var ya;
var f;
var alreadyclicked = false;
var punkt1 = [-2, -1.25];
var punkt2 = [0.5, 1.25];
var lastMousePosition = [0, 0];
var colors = ["#000000", "#55AA22", "#DD5599"];
var iterationen = 300;

function load() {
    var canvas = document.getElementById("canvasMandelbrot");
    var divControls = document.getElementById("divControls");
    var nav = document.getElementsByTagName("nav")[0];
    canvas.width = (window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth) - 20;
    canvas.height = (window.innerHeight || document.documentElement.clientHeight || document.body.clientHeight).toString() - (divControls.offsetHeight + nav.clientHeight + 20);
    canvas.style.top = nav.clientHeight.toString() + "px";
    divControls.style.top = (nav.clientHeight + canvas.height) + "px";

    getValues();

    canvas.addEventListener("click", function (event) {
        if (!alreadyclicked) {
            alreadyclicked = true;
            punkt1[0] = event.x * f + xa;
            punkt1[1] = (event.y - 25) * f + ya;
            lastMousePosition[0] = event.x;
            lastMousePosition[1] = (event.y - 25);
            fillPixel(event.x, (event.y - 25), canvas, "#FF0000");
        } else {
            alreadyclicked = false;
            drawLine(lastMousePosition[0], lastMousePosition[1], event.x, lastMousePosition[1], canvas, "#FF0000");
            drawLine(lastMousePosition[0], lastMousePosition[1], lastMousePosition[0], (event.y - 25), canvas, "#FF0000");
            drawLine(lastMousePosition[0], (event.y - 25), event.x, (event.y - 25), canvas, "#FF0000");
            drawLine(event.x, lastMousePosition[1], event.x, (event.y - 25), canvas, "#FF0000");
            punkt2[0] = event.x * f + xa;
            punkt2[1] = (event.y - 25) * f + ya;
        }
    }, false);

    start();

    setValues();
}

function drawMandelbrot(canvas, x1, y1, x2, y2, iterationen, colors) {
    /*Werte Definieren*/
    xa = x1;
    ya = y1;
    var ls = Math.abs(y2 - y1);
    f = ls / canvas.height;
    var xc;
    var yc;
    var xz;
    var yz;
    var xxz;
    var t;

    /*Schleifen*/
    for (var x = 1; x < canvas.width; x++) { //Alle Pixel auf der x-Achse durchlaufen
        for (var y = 1; y < canvas.height; y++) { //Alle Pixel auf der y-Achse durchlaufen
            xc = xa + x * f;
            yc = ya + y * f;
            xz = 0;
            yz = 0;

            for (var zaehler = 1; zaehler <= iterationen && Math.sqrt(xz * xz + yz * yz) < 2; zaehler++) {
                xxz = xz * xz - yz * yz + xc;
                yz = 2 * xz * yz + yc;
                xz = xxz;

                if (Math.sqrt(xz * xz + yz * yz) > 2) {
                    t = parseInt((parseInt((zaehler/2).toString())/(zaehler/2)).toString());
                    if (t === 0) {
                        fillPixel(x, y, canvas, colors[1]);
                    } else {
                        fillPixel(x, y, canvas, colors[2]);
                    }
                } else if (zaehler >= iterationen) {
                    fillPixel(x, y, canvas, colors[0]);
                }
            }
        }
    }
    console.debug("FERTIG! x1: " + x1 + " y1: " + y1 + " x2: " + x2 + " y2: " + y2);
    document.getElementById("labelProgress").innerHTML = "Fertig";
}

function fillPixel(x, y, canvas, color){
    canvas.getContext("2d").fillStyle = color;
    canvas.getContext("2d").fillRect(x,y,1,1);
}

function drawLine(x1, y1, x2, y2, canvas, color) {
    var ctx = canvas.getContext("2d");
    ctx.strokeStyle = color;
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.stroke();
}

function start() {
    document.getElementById("labelProgress").innerHTML = "Lade Mandelbrot...";
    setTimeout(function() {
        drawMandelbrot(document.getElementById("canvasMandelbrot"), punkt1[0], punkt1[1], punkt2[0], punkt2[1], iterationen, colors);
    }, 100);

    setValues();
}

function reset() {
    punkt1[0] = -2;
    punkt1[1] = -1.25;
    punkt2[0] = 0.5;
    punkt2[1] = 1.25;
    alreadyclicked = false;

    drawMandelbrot(document.getElementById("canvasMandelbrot"), punkt1[0], punkt1[1], punkt2[0], punkt2[1], iterationen, colors);

    setValues();
}

function getValues() {
    if (window.sessionStorage.getItem("iterationen") != undefined) {
        iterationen = parseInt(window.sessionStorage.getItem("iterationen"));
        colors[0] = window.sessionStorage.getItem("colors0");
        colors[1] = window.sessionStorage.getItem("colors1");
        colors[2] = window.sessionStorage.getItem("colors2");
    }

    if (window.sessionStorage.getItem("punkt10") != undefined) {
        punkt1[0] = parseFloat(window.sessionStorage.getItem("punkt10"));
        punkt1[1] = parseFloat(window.sessionStorage.getItem("punkt11"));
        punkt2[0] = parseFloat(window.sessionStorage.getItem("punkt20"));
        punkt2[1] = parseFloat(window.sessionStorage.getItem("punkt21"));
    }
}

function setValues() {
    window.sessionStorage.setItem("punkt10", punkt1[0].toString());
    window.sessionStorage.setItem("punkt11", punkt1[1].toString());
    window.sessionStorage.setItem("punkt20", punkt2[0].toString());
    window.sessionStorage.setItem("punkt21", punkt2[1].toString());
}