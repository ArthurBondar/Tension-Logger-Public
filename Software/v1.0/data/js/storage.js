
// display error on screen if failed to get a resource
function server_error() {
    document.getElementById("diskType").innerHTML = "server error!";
    document.getElementById("diskType").className = 'table-danger';
    console.error("couldn't get a resource form server");
}

function card_not_found() {
    document.getElementById("diskType").innerHTML = "not found";
    document.getElementById("diskSpace").innerHTML = "";
    document.getElementById("diskUsed").innerHTML = "";
    const table = document.createElement('tbody');
    const old_table = document.getElementById("directoryTable").getElementsByTagName('tbody')[0];
    old_table.parentNode.replaceChild(table, old_table);
}

// Add td element with text to a table row
function addTableElement(row, element, text) {
    let col = document.createElement(element);
    if (element == "a") {
        addClass(col, ['btn', 'btn-sm', 'm-1', 'text-light', 'align-items-center']);
        if (text == "Download") {
            addClass(col, ['btn-info']);
            let fileName = row.getElementsByTagName("td")[1].textContent;
            col.setAttribute("href", filePath + fileName);
            col.setAttribute("download", fileName);
        }
        else if (text == "Delete") {
            col.addEventListener("click", deleteFile);
            addClass(col, ['btn-danger']);
        }
    }
    col.innerHTML = text;
    row.appendChild(col);
}

// delete button clicked
async function deleteFile(e) {
    const file = e.target.parentNode.getElementsByTagName("td")[1].textContent;
    await fetch(filePath + file, {
        method: 'DELETE',
        cache: 'no-cache',
        credentials: 'same-origin',
        headers: { 'Content-Type': 'text/plain' },
        body: filePath + file
    })
        .then(response => {
            if (response.ok) {
                //console.log("resource deleted [" + response.status + "] server: " + response.text());
                //e.target.parentNode.remove();
            }
            else console.error("HTTP-Error: " + response.status);
        })
        .catch((error) => {
            console.error('Error:', error);
        });
    await listDir();
}

async function listDir() {
    let mem = await getDiskInfo();
    if (mem == 0) {
        return; // check if card inserted
    }
    let data = [];
    data = await getJSON(listDirURL);
    if (data == 0) { // clear table
        const tb = document.createElement('tbody');
        const old_tb = document.getElementById("directoryTable").getElementsByTagName('tbody')[0];
        old_tb.parentNode.replaceChild(tb, old_tb);
        return;
    }
    //console.log(data);// debug
    data.sort(function (a, b) { // sort by date
        return new Date(b.date) - new Date(a.date);
    });
    const table = document.createElement('tbody');
    for (let i = 0; i < data.length; i++) {
        const row = document.createElement("tr");
        addTableElement(row, "td", i + 1);
        addClass(row, ['text-center']);
        addTableElement(row, "td", data[i]['name']);
        addTableElement(row, "td", data[i]['date'].replace("T", " "));
        let fsize = data[i]['size'] / (1024 * 1024);
        let funit = (fsize > 1024) ? " GB" : " MB";
        fsize = (fsize > 1024) ? fsize / 1024 : fsize;
        addTableElement(row, "td", fsize.toFixed(2) + funit);
        addTableElement(row, "a", "Download");
        addTableElement(row, "a", "Delete");
        table.appendChild(row);
    }
    const old_table = document.getElementById("directoryTable").getElementsByTagName('tbody')[0];
    old_table.parentNode.replaceChild(table, old_table);
}

// populate the disk info page
async function getDiskInfo() {
    let card = await getJSON(getMemURL);
    if (card == 0) {
        server_error();
        return 0;
    }
    //console.log(card);
    if (card.present == true) {
        document.getElementById("diskType").innerHTML = card['cardtype'];
        let totalMem = card['totalmem'] / 1024;
        let usedMem = (card['totalmem'] - card['freemem']) / 1024;
        let percent = usedMem / totalMem * 100;
        let totalMemUnits = (totalMem > 1024) ? ' GB' : ' MB';
        let usedMemUnits = (usedMem > 1024) ? ' GB' : ' MB';
        totalMemFrmted = (totalMem > 1024) ? totalMem / 1024 : totalMem;
        usedMem = (usedMem > 1024) ? usedMem / 1024 : usedMem;
        document.getElementById("diskSpace").innerHTML = totalMemFrmted.toFixed(2) + totalMemUnits;
        document.getElementById("diskUsed").innerHTML = usedMem.toFixed(2) + usedMemUnits + ' (' + percent.toFixed(1) + '%)';
        return [usedMem, totalMem];
    } else {
        card_not_found();
        return 0;
    }
}

// main
(async () => {

    let info = await getJSON(infoURL) || 0;
    if (info != 0)
        setValue("toolbar-version", "Version " + info.version + " © 2020");

    let diskMem = await getDiskInfo();
    console.log(diskMem);
    if (diskMem == 0) diskMem = [0, 100];
    await listDir();

    // loading the pie chart
    var ctx2 = $('#memoryChart');
    var myPieChart = new Chart(ctx2, {
        type: 'pie',
        data: {
            datasets: [{
                data: diskMem,
                backgroundColor: ['#d9534f', '#0275d8'],
                borderWidth: 0
            }],
            labels: [
                'used',
                'free',
            ]
        },
        options: {
            legend: {
                display: false,
            },
            resonsive: true,
            cutoutPercentage: 50
        }
    });

})()