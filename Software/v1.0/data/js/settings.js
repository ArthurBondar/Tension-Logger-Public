
async function getSettings() {
    let _settings = await getJSON(getSettingsURL);
    if (_settings == 0) {
        print_error("Failed to get " + getSettingsURL + " from the server");
        return;
    }
    //console.log("got: ", _settings);
    setValueObject("settings-points", _settings, 'graph_points', '');
    setValueObject("settings-refresh", _settings, 'refresh_rate', ' sec');
    setValueObject("settings-setpoint", _settings, 'set_point', '');
    // storage
    setValueObject("settings-logging", _settings, 'interval', ' sec');
    // system
    let data = await getJSON(infoURL);
    if (data == 0) {
        print_error("Failed to get " + infoURL + " from the server");
        return;
    }
    //console.log(data);
    setValueObject("settings-version", data, 'version', '');
    document.getElementById("toolbar-version").innerHTML = "Version " + data.version + " © 2020";
    setValueObject("settings-coincell", data, 'coincell', ' V');
    setValueObject("settings-temperature", data, 'temperature', " C");
}

// get the parameter value from the input colum or from value column in input not entered
function getValue(parameter) {
    if (document.getElementById("settings-" + parameter + "-input").value == "") {
        if (parameter == "datetime")
            return document.getElementById("settings-" + parameter).innerHTML;
        else
            return document.getElementById("settings-" + parameter).innerHTML.split(" ")[0];
    } else {
        return (document.getElementById("settings-" + parameter + "-input").value).replace("T", " ");
    }
}

// check if a setting is in range, flag red if not
function checkSetting(param, min, max) {
    element = document.getElementById("settings-" + param + "-input");
    value = parseInt(getValue(param));
    if (value < min || value > max) {
        addClass(element, ["text-danger", "is-invalid"]);
        return true;
    } else {
        removeClass(element, ["text-danger", "is-invalid"]);
        return false;
    }
}

// check update button is pressed, runs thru the settings and sends json to server
async function setSettings(e) {
    // settings error checks
    let pass = false;
    pass = pass | checkSetting("points", 0, 100);
    pass = pass | checkSetting("refresh", 1, 1800);
    pass = pass | checkSetting("logging", 1, 1800);
    if (pass == true) return;

    settings = {
        //datetime: getValue("datetime"),
        graph_points: parseInt(getValue("points")),
        refresh_rate: parseInt(getValue("refresh")),
        set_point: parseInt(getValue("setpoint")),
        interval: parseInt(getValue("logging"))
    };
    //console.log(settings);
    await sendJSON(getSettingsURL, settings); // send new settings to the server
    await delay(250);
    await getSettings();    // read back and confirm new settings
}

// set system datetime
async function setDate() {
    datetime_obj = {
        datetime: getValue("datetime")
    }
    await sendJSON(datetimeURL, datetime_obj);
    await getDate();
}

// get datetime from the system
async function getDate() {
    let dt = await getJSON(datetimeURL);
    if (dt == 0) {
        print_error("Failed to get " + datetimeURL + " from the server");
        return;
    }
    setValueObject("settings-datetime", dt, "datetime", '');
    setTimeout(this.getDate, (10) * 1000);
}

// main
(async () => {
    document.getElementById("settings_button").addEventListener('click', async () => { await setSettings() });
    document.getElementById("datetime_button").addEventListener('click', async () => { await setDate() });
    await getSettings();
    await getDate();

})()