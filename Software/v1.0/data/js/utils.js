

feather.replace();

// URLS
// const getDataURL = "/index.php";
// const getSettingsURL = "/settings.php";
//const listDirURL = "/storage.php?type=listDir";
//const getMemURL = "/storage.php?type=memory";
// const deleteFileURL = "/storage.php";
// const filePath = "/data/";

const getDataURL = "/measurement";
const getSettingsURL = "/settings.json";
const listDirURL = "/listdir";
const getMemURL = "/memory";
const filePath = "/sdcard/";
const datetimeURL = "/datetime";
const infoURL = "/info";

// post json data to the server
async function sendJSON(url, data) {
    fetch(url, {
        method: 'POST',
        cache: 'no-cache',
        credentials: 'same-origin',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(data)
    })
        .then(response => {
            if (response.ok) {
                response.text().then(resp => {
                    //console.log("[" + response.status + "] server: " + resp);
                });
            }
            else console.error("HTTP-Error: " + response.status);
        })
        .catch((error) => {
            console.error('Error:', error);
        });
}

// get json data from the server
async function getJSON(url) {
    return fetch(url, {
        method: 'GET',
        cache: 'no-cache',
        credentials: 'same-origin',
    })
        .then(response => response.json())
        .then(data => {
            return data;
        })
        .catch((error) => {
            console.error('Error:', error);
            return 0;
        });
}

// add array of class tags to an element
function addClass(element, classList) {
    for (cl in classList) {
        element.classList.add(classList[cl]);
    }
}

// remove array of class tags to an element
function removeClass(element, classList) {
    for (cl in classList) {
        element.classList.remove(classList[cl]);
    }
}

// print error for the user
function print_error(str) {
    alert(str);
}

// delay
function delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

// setvalue in the HTML value in the settings parameter column
function setValueObject(element, obj, prop, units) {
    //console.log("elem:"+element+"prop:"+prop+" obj:"+JSON.stringify(obj);
    if (obj.hasOwnProperty(prop)) {
        const value = obj[prop];
        document.getElementById(element).innerHTML = value + units;
        document.getElementById(element).setAttribute("value", value);
    } else {
        console.log("field " + prop + " doesnt exists in object: " + String(obj));
    }
}

function setValue(element, value) {
        document.getElementById(element).innerHTML = value;
        document.getElementById(element).setAttribute("value", value);
}