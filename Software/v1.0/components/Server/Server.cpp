/*




*/

#include "Server.h"

static const char *TAG = "Server";

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

//
Server::Server(/* args */)
{
}

//
Server::~Server()
{
}

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html"))
        type = "text/html";
    else if (CHECK_FILE_EXTENSION(filepath, ".js"))
        type = "application/javascript";
    else if (CHECK_FILE_EXTENSION(filepath, ".css"))
        type = "text/css";
    else if (CHECK_FILE_EXTENSION(filepath, ".png"))
        type = "image/png";
    else if (CHECK_FILE_EXTENSION(filepath, ".ico"))
        type = "image/x-icon";
    else if (CHECK_FILE_EXTENSION(filepath, ".svg"))
        type = "text/xml";
    else if (CHECK_FILE_EXTENSION(filepath, ".csv"))
        type = "text/csv";
    return httpd_resp_set_type(req, type);
}

// Handler GET: /*
static esp_err_t common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/')
        strlcat(filepath, "/index.html", sizeof(filepath));
    else
        strlcat(filepath, req->uri, sizeof(filepath));

    FILE *fd = fopen(filepath, "r");
    if (fd == NULL)
    {
        ESP_LOGE(TAG, "common_get_handler(): failed to open - %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");

    char *chunk = rest_context->scratch;
    // increasing file buffer size
    if (setvbuf(fd, chunk, _IOFBF, SCRATCH_BUFSIZE) != 0)
        ESP_LOGE(TAG, "common_get_handler(): setvbuf failed"); // POSIX version sets errno

    ssize_t chunksize;
    do
    {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);
        if (chunksize > 0)
        {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK)
            {
                fclose(fd);
                ESP_LOGE(TAG, "common_get_handler(): failed sensing file %s", filepath);
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (chunksize != 0);
    /* Close file after sending complete */
    fclose(fd);
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// Handler GET: /sdcard/*
static esp_err_t data_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX], *file_name;
    SDCard *card = SDCard::instance();
    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, req->uri, sizeof(filepath));
    file_name = strtok(filepath, "/");
    file_name = strtok(NULL, "/");
    //ESP_LOGI(TAG, "data_get_handler(): second tok %s", file_name);

    if (card->mount() != ESP_OK)
    {
        ESP_LOGE(TAG, "data_get_handler(): failed to mount");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    if (card->openFile(file_name, "r") != ESP_OK)
    {
        ESP_LOGE(TAG, "data_get_handler(): failed to open %s", file_name);
        card->unmount();
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    char *chunk = rest_context->scratch;
    ssize_t chunksize;
    do
    {
        chunksize = card->readFile(chunk, SCRATCH_BUFSIZE);
        if (chunksize > 0)
        {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK)
            {
                ESP_LOGE(TAG, "common_get_handler(): failed sensing file %s", filepath);
                card->closeFile();
                card->unmount();
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (chunksize != 0);
    card->closeFile();
    card->unmount();
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// Handler: GET /measurement
static esp_err_t measurements_get_handler(httpd_req_t *req)
{
    /* {
        present: bool,
        message: str, (comma separated)
        color: str,
        timestamp: str,
        tension: int,
        units: str
    } */
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "error");
        return ESP_FAIL;
    }

    System *sys = System::instance();
    Sensor *sen = Sensor::instance();
    SensorData sen_data = SENSOR_DEFAULTS();
    char buff[ERROR_MSG_LEN];

    // Status field
    cJSON_AddBoolToObject(root, "present", (sys->getErrorFlag(sensor_not_found) == true) ? false : true);
    sys->getErrorMsg(buff, sizeof(buff));
    cJSON_AddStringToObject(root, "message", buff);
    sys->getErrorMsgColor(buff, sizeof(buff));
    cJSON_AddStringToObject(root, "color", buff);
    sen->getData(&sen_data);
    sys->getTimeString(buff, sizeof(buff), TIME_FORMAT_SEC, sen_data.timestamp);
    cJSON_AddStringToObject(root, "timestamp", buff);
    cJSON_AddNumberToObject(root, "tension", sen_data.tension);
    cJSON_AddStringToObject(root, "units", sen_data.units);

    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// Handler: POST /settings.json
static esp_err_t settings_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len, cur_len = 0, received = 0;
    char *buff = ((rest_server_context_t *)(req->user_ctx))->scratch;
    if (total_len >= SCRATCH_BUFSIZE)
    {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, buff + cur_len, total_len);
        if (received <= 0)
        {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed");
            ESP_LOGE(TAG, "settings_post_handler(): httpd_req_recv error");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buff[total_len] = '\0';
    //ESP_LOGI(TAG, "got: %s", buff);

    Settings *settings = Settings::instance();
    if (!settings->getInitialized())
        settings->init();
    cJSON *root = cJSON_Parse(buff);
    if (root == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed");
        ESP_LOGE(TAG, "settings_post_handler(): invalid json");
        return ESP_FAIL;
    }
    cJSON *element = NULL;
    cJSON_ArrayForEach(element, root)
    {
        if (cJSON_IsString(element))
            settings->setParameter(element->string, element->valuestring);
        else if (cJSON_IsNumber(element))
            settings->setParameter(element->string, element->valuedouble);
        else
            ESP_LOGE(TAG, "settings_post_handler(): settings[%s] unknown type", element->string);
    }
    cJSON_Delete(root);

    if (settings->saveToFlash() == ESP_FAIL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed");
        return ESP_FAIL;
    }
    memset(buff, 0, SCRATCH_BUFSIZE);
    settings->getSettingsString(buff, SCRATCH_BUFSIZE);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buff);
    //ESP_LOGI(TAG, "settings_post_handler(): settings = %s", buff);
    return ESP_OK;
}

// Hanlder: GET /memory
static esp_err_t memory_get_handler(httpd_req_t *req)
{
    /* {
            present: bool,
            cardtype: str,
            totalmem: str,
            freemem: int,
        }*/
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "error");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    SDCard *card = SDCard::instance();
    SDCardSpace card_mem;
    card_mem.cardSize = 0;
    card_mem.totalBytes = 0;
    card_mem.freeBytes = 0;
    char buff[25];

    esp_err_t card_in = card->mount();
    if (card_in == ESP_OK)
    {
        cJSON_AddBoolToObject(root, "present", true);
        card->getCardSpace(&card_mem);
        card->unmount();
        cJSON_AddStringToObject(root, "cardtype", card_mem.name);
        snprintf(buff, sizeof(buff), "%llu", card_mem.totalBytes);
        cJSON_AddStringToObject(root, "totalmem", buff);
        snprintf(buff, sizeof(buff), "%llu", card_mem.freeBytes);
        cJSON_AddStringToObject(root, "freemem", buff);
    }
    else
    {
        cJSON_AddBoolToObject(root, "present", false);
        cJSON_AddStringToObject(root, "cardtype", "not found");
        cJSON_AddStringToObject(root, "totalmem", "0");
        cJSON_AddStringToObject(root, "freemem", "0");
    }

    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// Hanlder: GET /datetime
static esp_err_t datetime_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "error");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    System *sys = System::instance();
    char buff[TIME_LEN];
    memset(buff, 0, sizeof(buff));
    tm sys_date;

    sys->getTime(&sys_date);
    sys->getTimeString(buff, sizeof(buff), TIME_FORMAT, sys_date);
    cJSON_AddStringToObject(root, "datetime", buff);

    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// Hanlder: GET /info
static esp_err_t info_get_handler(httpd_req_t *req)
{
    /* {
            coincell: str,
            temperature: num,
            version: str,
     }*/
    System *sys = System::instance();
    char buff[20];
    memset(buff, 0, sizeof(buff));

    if (sys->updateTemp() != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to get temperature");
        ESP_LOGE(TAG, "info_get_handler(): failed to get temperature");
        return ESP_FAIL;
    }
    sys->updateVoltage();

    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "error");
        return ESP_FAIL;
    }
    snprintf(buff, sizeof(buff), "%.2f", sys->getVoltage());
    cJSON_AddStringToObject(root, "coincell", buff);
    cJSON_AddNumberToObject(root, "temperature", sys->getTemp());
    cJSON_AddStringToObject(root, "version", sys->getVersion());

    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// Handler: POST /datetime
static esp_err_t datetime_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buff = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE)
    {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, buff + cur_len, total_len);
        if (received <= 0)
        {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed");
            ESP_LOGE(TAG, "datetime_post_handler(): httpd_req_recv error");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buff[total_len] = '\0';
    //ESP_LOGI(TAG, "got: %s", buff);

    char *ch;
    tm new_time;
    cJSON *root = cJSON_Parse(buff);
    if (root == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed");
        ESP_LOGE(TAG, "datetime_post_handler(): setting time error");
        return ESP_FAIL;
    }

    cJSON *new_time_obj = cJSON_GetObjectItem(root, "datetime");
    if (new_time_obj == NULL)
        goto error;

    ch = cJSON_GetStringValue(new_time_obj);
    if (ch == NULL)
        goto error;

    if (System::instance()->getTimeStruct(&new_time, ch) != ESP_OK)
        goto error;

    if (System::instance()->setTime(new_time) != ESP_OK)
        goto error;

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;

error:
    cJSON_Delete(root);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed");
    ESP_LOGE(TAG, "datetime_post_handler(): setting time error");
    return ESP_FAIL;
}

// Handler: GET /listdir
// [ { name: str, date: str, size: int }, ... ]
static esp_err_t listdir_get_handler(httpd_req_t *req)
{
    // Getting file list from SD card
    SDCard *card = SDCard::instance();
    SDCardFile *files[MAX_FILE_LIST];
    int file_num = -1;

    if (card->mount() != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "card not found");
        return ESP_FAIL;
    }
    else
    {
        if (card->listDir(&files[0], &file_num) != ESP_OK)
        {
            card->unmount();
            ESP_LOGI(TAG, "listdir_get_handler(): ListDir Failed");
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "error");
            return ESP_FAIL;
        }
    }
    card->unmount();

    // Creating JSON array
    cJSON *dir_list = NULL, *dir = NULL;
    char buff[TIME_LEN];
    dir_list = cJSON_CreateArray();
    if (dir_list == NULL)
    {
        ESP_LOGI(TAG, "create JSON failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "error");
        SDCard::instance()->clearFileList();
        return ESP_FAIL;
    }
    for (int i = 0; i < file_num; i++)
    {
        dir = cJSON_CreateObject();
        if (dir == NULL)
        {
            ESP_LOGI(TAG, "create JSON failed");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "error");
            cJSON_Delete(dir_list);
            SDCard::instance()->clearFileList();
            return ESP_FAIL;
        }
        cJSON_AddStringToObject(dir, "name", files[i]->name);
        System::instance()->getTimeString(buff, sizeof(buff), TIME_FORMAT_JS, files[i]->lastWrite);
        cJSON_AddStringToObject(dir, "date", buff);
        cJSON_AddNumberToObject(dir, "size", files[i]->size);
        cJSON_AddItemToArray(dir_list, dir);
    }

    SDCard::instance()->clearFileList();
    const char *json_str = cJSON_Print(dir_list);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(dir_list);
    return ESP_OK;
}

// Handler: DELETE /sdcard/*
static esp_err_t data_delete_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX], *file_name;
    SDCard *card = SDCard::instance();
    strlcpy(filepath, req->uri, sizeof(filepath));
    file_name = strtok(filepath, "/");
    file_name = strtok(NULL, "/");

    if (card->mount() != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Card not found");
        return ESP_FAIL;
    }

    if (card->deleteFile(file_name) != ESP_OK)
    {
        //ESP_LOGE(TAG, "data_delete_handler(): failed remove file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        card->unmount();
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Failed to remove file");
        ESP_LOGE(TAG, "data_delete_handler(): failed to remove %s", file_name);
        return ESP_FAIL;
    }
    card->unmount();
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

//
esp_err_t Server::start_server(const char *base_path)
{
    if (!base_path)
    {
        ESP_LOGE(TAG, "%s wrong base path", base_path);
        return ESP_FAIL;
    }

    this->rest_context = (rest_server_context_t *)calloc(1, sizeof(rest_server_context_t));
    if (!this->rest_context)
    {
        ESP_LOGE(TAG, "No memory for rest context");
        return ESP_FAIL;
    }

    strlcpy(this->rest_context->base_path, base_path, sizeof(this->rest_context->base_path));
    memset(this->rest_context->scratch, 0, SCRATCH_BUFSIZE);

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.core_id = (BaseType_t) 1;
    config.task_priority = configMAX_PRIORITIES - 4;
    config.stack_size = 16384;
    config.max_uri_handlers = 10;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.backlog_conn = 10;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP Server");
    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Start server failed");
        free(this->rest_context);
        return ESP_FAIL;
    }

    /* URI handler for getting tension data */
    httpd_uri_t measurements_get_uri = {
        .uri = "/measurement",
        .method = HTTP_GET,
        .handler = &measurements_get_handler,
        .user_ctx = this->rest_context};
    httpd_register_uri_handler(server, &measurements_get_uri);

    /* URI handler for updating settings file */
    httpd_uri_t settings_post_uri = {
        .uri = "/settings.json",
        .method = HTTP_POST,
        .handler = &settings_post_handler,
        .user_ctx = this->rest_context};
    httpd_register_uri_handler(server, &settings_post_uri);

    /* URI handler for getting SD card info */
    httpd_uri_t memory_get_uri = {
        .uri = "/memory",
        .method = HTTP_GET,
        .handler = &memory_get_handler,
        .user_ctx = this->rest_context};
    httpd_register_uri_handler(server, &memory_get_uri);

    /* URI handler for getting system datetime */
    httpd_uri_t datetime_get_uri = {
        .uri = "/datetime",
        .method = HTTP_GET,
        .handler = &datetime_get_handler,
        .user_ctx = this->rest_context};
    httpd_register_uri_handler(server, &datetime_get_uri);

    /* URI handler for getting system datetime */
    httpd_uri_t datetime_post_uri = {
        .uri = "/datetime",
        .method = HTTP_POST,
        .handler = &datetime_post_handler,
        .user_ctx = this->rest_context};
    httpd_register_uri_handler(server, &datetime_post_uri);

    /* URI handler for getting system temp and coincell */
    httpd_uri_t info_get_uri = {
        .uri = "/info",
        .method = HTTP_GET,
        .handler = &info_get_handler,
        .user_ctx = this->rest_context};
    httpd_register_uri_handler(server, &info_get_uri);

    /* URI handler for listing directories on SD card */
    httpd_uri_t listdir_get_uri = {
        .uri = "/listdir",
        .method = HTTP_GET,
        .handler = &listdir_get_handler,
        .user_ctx = this->rest_context};
    httpd_register_uri_handler(server, &listdir_get_uri);

    /* URI handler for deliting file on SD card */
    httpd_uri_t data_delete_uri = {
        .uri = "/sdcard/*",
        .method = HTTP_DELETE,
        .handler = &data_delete_handler,
        .user_ctx = this->rest_context};
    httpd_register_uri_handler(server, &data_delete_uri);

    /* URI handler for getting web server files */
    httpd_uri_t data_get_uri = {
        .uri = "/sdcard/*",
        .method = HTTP_GET,
        .handler = &data_get_handler,
        .user_ctx = this->rest_context};
    httpd_register_uri_handler(server, &data_get_uri);

    /* URI handler for getting web server files */
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = &common_get_handler,
        .user_ctx = this->rest_context};
    httpd_register_uri_handler(server, &common_get_uri);

    return ESP_OK;
}

//
esp_err_t Server::init()
{
    if (!System::instance()->spiffs_initialized())
    {
        if (System::instance()->init_spiffs() != ESP_OK)
        {
            return ESP_FAIL;
        }
    }
    if (this->start_server(WEB_MOUNT_POINT) != ESP_OK)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}