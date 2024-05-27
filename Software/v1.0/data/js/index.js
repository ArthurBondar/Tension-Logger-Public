
var DataPoints = {
    index: 0,
    data: [],
    setpoint: [],
    timestamp: [],
};

function createChart(settings) {
    return new Chart($('#myChart'), {
        type: 'line',
        data: {
            labels: ["0:00:00"],
            datasets: [{
                label: "load",
                data: [0],
                fill: false,
                backgroundColor: 'transparent',
                borderColor: '#007bff',
                borderCapStyle: 'round',
                borderJoinStyle: 'round',
                cubicInterpolationMode: 'monotone',
                lineTension: 0.1,
                borderWidth: 3,
                pointRadius: 3,
                pointHoverRadius: 6,
                pointBackgroundColor: '#007bff'
            },
            {
                label: "full",
                data: [settings.set_point],
                lineTension: 0,
                fill: false,
                backgroundColor: 'transparent',
                borderColor: '#ff0000',
                borderCapStyle: 'round',
                borderJoinStyle: 'round',
                cubicInterpolationMode: 'monotone',
                lineTension: 0.1,
                borderWidth: 3,
                pointRadius: 3,
                pointHoverRadius: 6,
                pointBackgroundColor: '#007bff'
            }]
        },
        options: {
            responsive: true,
            animation: {
                duration: 0 // general animation time
            },
            scales: {
                yAxes: [{
                    ticks: {
                        beginAtZero: true
                    }
                }]
            },
            legend: {
                display: false,
            }
        }
    });
}

// push dataset to the chart widget
function addData(chart, labels, data1, data2) {
    chart.data.labels = labels.concat();
    chart.data.datasets[0].data = data1.concat();
    chart.data.datasets[1].data = data2.concat();
    chart.update();
}

// get data from the server and update the HTML screen
async function getData(LineChart, settings) {

    let data = await getJSON(getDataURL);
    //console.log(data);
    if (data != 0) {
        if (data.hasOwnProperty('message')) {
            let msg_arr = data.message.split(',');
            let str = "";
            for (var i = 0; i < msg_arr.length - 1; i++)
                str += msg_arr[i] + "<br>";
            document.getElementById("Status").innerHTML = str;
        }
        //setValueObject("Status", data, 'message', '');
        if (data.hasOwnProperty('color')) {
            document.getElementById("Status").parentNode.className = 'table-' + data['color'];
        }
        if (data.hasOwnProperty('present')) {
            if (data['present'] == false) {
                setTimeout(this.getData, (settings.refresh_rate) * 1000, LineChart, settings);
                return;
            }
        }
        setValueObject('Date', data, 'timestamp', '');
        if (!data.hasOwnProperty('tension')) {
            console.error(getDataURL + " has no property [tension]");
            setTimeout(this.getData, (settings.refresh_rate) * 1000, LineChart, settings);
            return;
        }
        const ten = data.tension;
        let perc = Math.round(ten / settings.set_point * 100);
        perc = (perc > 100) ? 100 : perc;
        setValue("Tension", ten + " / " + settings.set_point + " " + data.units + " (" + perc + "%)");
        document.getElementById("progressBar").setAttribute("aria-valuenow", perc);
        document.getElementById("progressBar").setAttribute("style", "width: " + perc + "%");
        if (perc < 33) document.getElementById("progressBar").className = "progress-bar progress-bar-striped progress-bar-animated bg-warning";
        else if (perc < 66) document.getElementById("progressBar").className = "progress-bar progress-bar-striped progress-bar-animated";
        else document.getElementById("progressBar").className = "progress-bar progress-bar-striped progress-bar-animated bg-success";
        // add data to the chart
        if (DataPoints.index >= settings.graph_points) {
            DataPoints.data.shift();
            DataPoints.timestamp.shift();
            DataPoints.setpoint.shift();
        } else {
            DataPoints.index++;
        }
        DataPoints.data.push(data.tension);
        DataPoints.timestamp.push(data.timestamp.split(" ")[1]);
        DataPoints.setpoint.push(settings.set_point);
        //console.log(DataPoints);
        if (settings.graph_points != 0) {
            addData(LineChart, DataPoints.timestamp, DataPoints.data, DataPoints.setpoint);
        }
        setTimeout(this.getData, (settings.refresh_rate) * 1000, LineChart, settings);
    } else {
        console.error('getData() failed');
        document.getElementById("Status").innerHTML = "failed to get data!";
        document.getElementById("Status").parentNode.className = 'table-danger';
    }
}

// main
(async () => {

    let info = await getJSON(infoURL) || 0;
    if (info != 0)
        setValue("toolbar-version", "Version " + info.version + " © 2020");

    let settings = await getJSON(getSettingsURL) || { graph_points: 0, set_point: 0 };
    //console.log(settings);
    let LineChart = {};
    if (settings.graph_points != 0) {
        LineChart = createChart(settings);
    }
    await getData(LineChart, settings);

})()