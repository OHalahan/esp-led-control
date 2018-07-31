var rainbowEnable = false;
var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);
connection.onopen = function () {
    connection.send('Connect ' + new Date());
};
connection.onerror = function (error) {
    console.log('WebSocket Error ', error);
};
connection.onmessage = function (e) {  
    console.log('Server: ', e.data);
    document.getElementById('r').value = Number(e.data);
    sendRGB();
};
connection.onclose = function(){
    console.log('WebSocket connection closed');
};

function sendRGB(data = document.getElementById('r').value) {
    console.log('Value: ' + data); 
    console.log('Sending object');
    connection.send(JSON.stringify({fadeIn: "1", fadeOut: "1", startBr: data, endBr: data}));
}

function sendSpecial(fadeIn, fadeOut, start, end) {
    connection.send(JSON.stringify({fadeIn: fadeIn, fadeOut: fadeOut, startBr: start, endBr: end}));
}