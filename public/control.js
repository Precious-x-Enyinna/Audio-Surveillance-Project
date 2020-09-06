var socket = io();
var audio = document.getElementById("audio");
var ctrl = document.getElementById("controls");
var restart = document.getElementById("restart");
var stopAudio = document.getElementById("stop");
var fileNum = 0;
var currentFile;
var awaitingFile = true;
var loadedFileSave = 0;
var pauseIcon =
    '<a href="#" class="tooltipped" data-tooltip="pause"><i class="material-icons">pause_circle_outline</i></a>';
var playIcon =
    '<a href="#" class="tooltipped" data-tooltip="pause"><i class="material-icons">play_circle_outline</i></a>';

var canvasWidth = 300;
//this sets the canvas element as 2d
var canvas = document.getElementById("progress").getContext('2d');

audio.addEventListener('loadedmetadata', function() {
    var duration = audio.duration;
    var currentTime = audio.currentTime;
    //convertElapsedTime function will be declared below
    document.getElementById("duration").innerHTML = convertElapsedTime(duration);
    document.getElementById("current-time").innerHTML = convertElapsedTime(currentTime);

    canvas.fillRect(0, 0, canvasWidth, 50);
});

ctrl.onclick = function() {
    var pause = ctrl.innerHTML === pauseIcon;
    ctrl.innerHTML = pause ? playIcon : pauseIcon;

    var method = pause ? "pause" : "play";

    audio[method]();
};

var rewind = document.getElementById("rewind");
var forward = document.getElementById("forward");
var fullAudioBtn = document.getElementById("fullAudio");
rewind.onclick = function() {
    audio.currentTime -= 2;
};
forward.onclick = function() {
    audio.currentTime += 2;
};
restart.onclick = function() {
    audio.currentTime = 0;
};
fullAudioBtn.onclick = function() {
        document.getElementById("audioPlayer").innerHTML = '<audio src="/secondary/main.wav" controls></audio>'
    }
    // stopAudio.onclick = function() {
    //     alert("heyyyy")
    // };

//********************************************************/
$(document).ready(function() {
    document.getElementById("loading").innerHTML = '<i class="fa fa-circle-o-notch fa-spin"></i>';
    $(function() {
        socket.on('play', function() {
            playAudio();

        })
        socket.on('pause', function() {
            pauseAudio();
        })

        socket.on('load', (loadedFiles) => {
            if (awaitingFile) {
                console.log("waiting for file");
                loadAudio(loadedFiles);
            } else {
                if (audio.paused) {
                    playAudio();
                }
            }

            loadedFileSave = loadedFiles;
            console.log("load");
        })
    })

    // $('#audio').on('play', function() {
    //     socket.broadcast.emit('play');
    //     return false;
    // })
    // $('#audio').on('pause', function() {
    //     socket.broadcast.emit('pause');
    //     return false;
    // })
    $('#stop').on('click', function() {
        socket.broadcast.emit('stop');
        return false;
    })
});

var playorpause = function() {
    if (audio.paused) {
        audio.play();
    } else {
        audio.pause();
    }
}

var playAudio = function() {
    audio.play();
    ctrl.innerHTML = pauseIcon;
}

var pauseAudio = function() {
    audio.pause();
    ctrl.innerHTML = playIcon;
}

var loadAudio = function(loadedFiles) {
    if (loadedFiles > fileNum) {
        fileNum++;
        currentFile = "file" + fileNum.toString() + ".wav";
        audio.src = "/Secondary/" + currentFile;
        awaitingFile = false;
        document.getElementById("loading").innerHTML = "";
        document.getElementById("nameOfAudioPlaying").innerHTML = "Currently playing " + audio.src;
        audio.load();
    } else {
        pauseAudio();
        document.getElementById("loading").innerHTML = '<i class="fa fa-circle-o-notch fa-spin"></i>';
        awaitingFile = true;
    }
}

var loadNext = function() {
    loadAudio(loadedFileSave);
    console.log("load");
    playAudio();
    console.log("playing" + audio.src)
}

function updateBar() {
    canvas.clearRect(0, 0, canvasWidth, 50);
    canvas.fillStyle = "#000";
    canvas.fillRect(0, 0, canvasWidth, 50)
        //the above block of code works like this...on each progression of the song, the canvas is cleared,set the style back to black (the default), and fill it with the size of the canvas

    var duration = audio.duration;
    var currentTime = audio.currentTime;

    //this changes the button icon back to play when the audio
    if (currentTime === duration) {
        ctrl.innerHTML = playIcon;
    }

    document.getElementById("current-time").innerHTML = convertElapsedTime(currentTime);

    var percentage = currentTime / duration;
    var progress = (canvasWidth * percentage);
    canvas.fillStyle = "#eeeeee";
    canvas.fillRect(0, 0, progress, 50);
}


function convertElapsedTime(inputSeconds) {
    var seconds = Math.floor(inputSeconds % 60)
    if (seconds < 10) {
        seconds = "0" + seconds
    }
    var minutes = Math.floor(inputSeconds / 60)
    return minutes + ":" + seconds
}

function updateBar() {
    canvas.clearRect(0, 0, canvasWidth, 50);
    canvas.fillStyle = "#000";
    canvas.fillRect(0, 0, canvasWidth, 50)
        //the above block of code works like this...on each progression of the song, the canvas is cleared,set the style back to black (the default), and fill it with the size of the canvas

    var duration = audio.duration;
    var currentTime = audio.currentTime;

    //this changes the button icon back to play when the audio
    if (currentTime === duration) {
        ctrl.innerHTML = playIcon;
    }

    document.getElementById("current-time").innerHTML = convertElapsedTime(currentTime);

    var percentage = currentTime / duration
    var progress = (canvasWidth * percentage)
    canvas.fillStyle = "#eeeeee"
    canvas.fillRect(0, 0, progress, 50)
}


function convertElapsedTime(inputSeconds) {
    var seconds = Math.floor(inputSeconds % 60)
    if (seconds < 10) {
        seconds = "0" + seconds
    }
    var minutes = Math.floor(inputSeconds / 60)
    return minutes + ":" + seconds
}