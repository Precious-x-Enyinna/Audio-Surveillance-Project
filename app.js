var express = require('express');
var fs = require("fs");
var net = require('net');
var events = require('events');

const settings = {
    ip: "192.168.43.131" /* "192.168.43.75" */ ,
    port: 81
};


var samplesLength = 1000;
var sampleRate = 8000;
var mainStream = fs.createWriteStream(__dirname + "/public/Secondary/main.wav");
var fileNum = 1;
var fileToDelete = 0;
var secStreamName = __dirname + "/public/Secondary/file" + fileNum.toString() + ".wav";
var secStream = fs.createWriteStream(secStreamName);

var writeHeader = function(currStream) {
    var b = new Buffer.alloc(1024);
    b.write('RIFF', 0);
    /* file length */
    b.writeUInt32LE(4 + (8 + 16) + (8 + (8000 * 30 * 2 * 2)), 4);
    //b.writeUint32LE(0, 4);

    b.write('WAVE', 8);
    /* format chunk identifier */
    b.write('fmt ', 12);

    /* format chunk length */
    b.writeUInt32LE(16, 16);

    /* sample format (raw) */
    b.writeUInt16LE(1, 20);

    /* channel count */
    b.writeUInt16LE(1, 22);

    /* sample rate */
    b.writeUInt32LE(sampleRate, 24);

    /* byte rate (sample rate * block align) */
    b.writeUInt32LE(sampleRate * 2, 28);

    /* block align (channel count * bytes per sample) */
    b.writeUInt16LE(2, 32);

    /* bits per sample */
    b.writeUInt16LE(16, 34);

    /* data chunk identifier */
    b.write('data', 36);

    /* data chunk length */
    //b.writeUInt32LE(40, samplesLength * 2);
    b.writeUInt32LE('0xFFFFFFFF', 40);


    currStream.write(b.slice(0, 50));
};
writeHeader(mainStream);
writeHeader(secStream);

var app = express();
var http = require('http').createServer(app);
var io = require('socket.io')(http);

process.env.PWD = process.cwd()
app.use(express.static(process.env.PWD + '/public'));

//First Web page rendering, start recording here
var pageRendered = false;
var pageRender = new events.EventEmitter();
var controlAudio = new events.EventEmitter();
var playing = false;

app.get('/', function(req, res) {
    //start listening for data when page is rendered
    res.sendFile(__dirname + '/index.html');
    pageRendered = true;
    pageRender.emit('rendered')
});

io.on('connection', (socket) => {
    console.log('a user connected');
    //Get info from webpage such as played, pause or stop
    socket.on('played', function() {
        playing = true;
        console.log("played");
    });
    socket.on('paused', function() {
        playing = false;
        console.log('paused');
    });
    socket.on('stopped', function() {
        playing = false;
        console.log('stopped');
        endStream();
    });

    //Send commands to web page
    controlAudio.on('play', function() {
        socket.broadcast.emit('play');
        console.log('played');
    });
    controlAudio.on('pause', function() {
        socket.broadcast.emit('pause');
        console.log('paused');
    });
    controlAudio.on('load', function() {
        socket.broadcast.emit('load', fileNum - 1);
        // console.log('load');
    });
    socket.on('disconnect', () => {
        //endStream();
        console.log('user disconnected');
    });
});

http.listen(3000);


var client = new net.Socket();
console.log("connecting...");
client.connect(settings.port, settings.ip, function() {
    client.setNoDelay(true);
    console.log("Trying to connect");
    client.write("This is the client speaking...");

    client.on("data", function(data) {
        try {
            //console.log("GOT DATA");
            secStream.write(data);
            mainStream.write(data);
            //mainStream.flush();
            //console.log("got chunk of " + data.length);
        } catch (ex) {
            console.error("Er!" + ex);
        }
    });
});


var switchFile;
//start interval after page has rendered
pageRender.on('rendered', function() {
    setTimeout(function() {
        if (switchFile != null) {
            clearInterval(switchFile);
        }
        fs.renameSync(__dirname + "/public/Secondary/file" + fileNum.toString() + ".wav", __dirname + "/public/Secondary/file1.wav");
        controlAudio.emit('load');
        fileNum = 1;
        fileToDelete = 1;
        console.log("Page Rendered");
        switchFile = setInterval(function() {
            writeToNext();
        }, 10 * 1000);
    }, 1000)

});


function writeToNext() {
    secStream.end();
    fileNum++;
    controlAudio.emit('load');
    console.log("Loaded " + secStreamName);
    if (fileNum > 20) {
        fileToDelete++;
        fs.unlink(__dirname + "/public/Secondary/file" + fileToDelete.toString() + ".wav", (err) => {});
    }
    secStreamName = __dirname + "/public/Secondary/file" + fileNum.toString() + ".wav";
    secStream = fs.createWriteStream(secStreamName);
    writeHeader(secStream);
}

function endStream() {
    clearInterval(switchFile);
    mainStream.end();
    console.log('Done recording');
    //process.exit(0);  
    client.end();
}