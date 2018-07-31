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
    connection.send(data);
}
