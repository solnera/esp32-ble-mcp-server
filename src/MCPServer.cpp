#include "MCPServer.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "McpBle.h"
#include "mcp_transport.h"
#include "esp_log.h"

#define TAG "MCP_SERVER"

BLEMCPServer* BLEMCPServer::s_bound = nullptr;
bool BLEMCPServer::s_initialized = false;

String Properties::toString() const {
    DynamicJsonDocument doc(4096);
    JsonObject obj = doc.to<JsonObject>();
    toJson(obj);
    String result;
    serializeJson(doc, result);
    return result;
}

void Properties::toJson(JsonObject& obj) const {
    obj["type"] = type;

    if (title.length() > 0) {
        obj["title"] = title;
    }

    if (description.length() > 0) {
        obj["description"] = description;
    }

    if (!properties.empty()) {
        JsonObject propertiesObj = obj["properties"].to<JsonObject>();
        for (const auto& kv : properties) {
            const String& key = kv.first;
            const Properties& value = kv.second;
            JsonObject propObj = propertiesObj[key].to<JsonObject>();
            value.toJson(propObj);
        }
    }

    if (!required.empty()) {
        JsonArray requiredArray = obj["required"].to<JsonArray>();
        for (const auto& req : required) {
            requiredArray.add(req);
        }
    }

    if (hasAdditionalProperties) {
        obj["additionalProperties"] = additionalProperties;
    }

    if (items) {
        JsonObject itemsObj = obj["items"].to<JsonObject>();
        items->toJson(itemsObj);
    }

    if (!enumValues.empty()) {
        JsonArray enumArray = obj["enum"].to<JsonArray>();
        for (const auto& value : enumValues) {
            enumArray.add(value);
        }
    }

    if (!oneOf.empty()) {
        JsonArray oneOfArray = obj["oneOf"].to<JsonArray>();
        for (const auto& schema : oneOf) {
            JsonObject schemaObj = oneOfArray.createNestedObject();
            schema.toJson(schemaObj);
        }
    }

    if (!anyOf.empty()) {
        JsonArray anyOfArray = obj["anyOf"].to<JsonArray>();
        for (const auto& schema : anyOf) {
            JsonObject schemaObj = anyOfArray.createNestedObject();
            schema.toJson(schemaObj);
        }
    }

    if (!allOf.empty()) {
        JsonArray allOfArray = obj["allOf"].to<JsonArray>();
        for (const auto& schema : allOf) {
            JsonObject schemaObj = allOfArray.createNestedObject();
            schema.toJson(schemaObj);
        }
    }

    if (format.length() > 0) {
        obj["format"] = format;
    }

    if (defaultValue.length() > 0) {
        obj["default"] = defaultValue;
    }
}

String Tool::toString() const {
    DynamicJsonDocument doc(4096);
    JsonObject obj = doc.to<JsonObject>();

    obj["name"] = name;
    obj["description"] = description;

    JsonObject inputSchemaObj = obj["inputSchema"].to<JsonObject>();
    inputSchema.toJson(inputSchemaObj);

    if (outputSchema.type.length() > 0) {
        JsonObject outputSchemaObj = obj["outputSchema"].to<JsonObject>();
        outputSchema.toJson(outputSchemaObj);
    }

    String result;
    serializeJson(doc, result);
    return result;
}

BLEMCPServer::BLEMCPServer(const String& name, const String& version, const String& instructions)
    : serverName(name), serverVersion(version), serverInstructions(instructions) {
}

void BLEMCPServer::begin() {
    if (s_bound && s_bound != this) {
        Serial.println("MCP Server already bound");
        return;
    }
    s_bound = this;

    if (!rx_queue) {
        rx_queue = xQueueCreate(4, sizeof(char*));
    }
    if (!task_handle) {
        xTaskCreate(BLEMCPServer::taskEntry, "mcp_ble_rx", 4096, this, 1, &task_handle);
    }

    mcp_transport_set_sleep_fn(BLEMCPServer::sleepTicks, NULL);

    if (!s_initialized) {
        mcp_transport_init();
        mcp_transport_set_send_fn(BLEMCPServer::sendBytes, NULL);
        mcp_transport_set_message_cb(BLEMCPServer::onMessage, this);
        mcp_transport_set_tx_gap_ticks(1);
        mcp_transport_set_send_retry(3, 1);

        // Bind McpBle callbacks
        McpBle::getInstance().setRxCallback([](const uint8_t* data, size_t len) {
            mcp_transport_receive(data, len);
        });
        
        McpBle::getInstance().setMtuCallback(BLEMCPServer::onMtu);
        mcp_transport_set_mtu(McpBle::getInstance().getMtu());
        
        McpBle::getInstance().init();

        delay(1000);
        Serial.println("MCP over BLE Server Started");

        s_initialized = true;
    } else {
        mcp_transport_set_message_cb(BLEMCPServer::onMessage, this);
    }
}

void BLEMCPServer::loop() {
    if (!rx_queue) return;
    while (true) {
        char* msg = nullptr;
        if (xQueueReceive(rx_queue, &msg, 0) != pdTRUE) break;
        if (msg) {
            processMessage(msg);
            free(msg);
        }
    }
}

void BLEMCPServer::taskEntry(void* ctx) {
    auto* self = static_cast<BLEMCPServer*>(ctx);
    for (;;) {
        if (!self || !self->rx_queue) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }
        char* msg = nullptr;
        if (xQueueReceive(self->rx_queue, &msg, portMAX_DELAY) == pdTRUE && msg) {
            self->processMessage(msg);
            free(msg);
        }
    }
}

void BLEMCPServer::onMessage(const char* message, void* ctx) {
    if (!ctx) return;
    auto* self = static_cast<BLEMCPServer*>(ctx);
    if (!self->rx_queue || !message) return;
    size_t n = strlen(message);
    char* copy = (char*)malloc(n + 1);
    if (!copy) return;
    memcpy(copy, message, n);
    copy[n] = '\0';
    if (xQueueSend(self->rx_queue, &copy, 0) != pdTRUE) {
        free(copy);
    }
}

int BLEMCPServer::sendBytes(const uint8_t* data, size_t len, void* ctx) {
    (void)ctx;
    return McpBle::getInstance().sendNotification(data, len) ? 0 : -1;
}

void BLEMCPServer::onMtu(uint16_t mtu) {
    mcp_transport_set_mtu(mtu);
}

void BLEMCPServer::sleepTicks(uint32_t ticks, void* ctx) {
    (void)ctx;
    if (ticks > 0) delay(ticks * portTICK_PERIOD_MS);
}

void BLEMCPServer::logFn(int level, const char* tag, const char* message, void* ctx) {
    (void)ctx;
    if (!tag || !message) return;
    Serial.printf("[%s] %s\n", tag, message);
}

void BLEMCPServer::RegisterTool(const Tool& tool) {
    tools[tool.name] = tool;
    Serial.printf("Tool registered: %s\n", tool.name.c_str());
}

MCPRequest BLEMCPServer::parseRequest(const std::string& json) {
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, json);

    MCPRequest request;

    if (error) {
        request.method = "";
        return request;
    }

    request.method = doc["method"].as<std::string>();
    request.idDoc.set(doc["id"]);
    request.paramsDoc.set(doc["params"]);
    return request;
}

std::string BLEMCPServer::serializeResponse(const MCPResponse& response) {
    DynamicJsonDocument doc(8192);
    JsonVariantConst idVariant = response.id();
    if (idVariant.is<const char*>() || idVariant.is<String>()) {
        doc["id"] = idVariant.as<String>();
    } else if (idVariant.is<int>() || idVariant.is<long>() || idVariant.is<unsigned int>() || idVariant.is<unsigned long>()) {
        doc["id"] = idVariant.as<long>();
    } else if (idVariant.isNull()) {
        doc["id"] = nullptr;
    } else {
        doc["id"] = idVariant;
    }
    doc["jsonrpc"] = "2.0";

    if (response.hasResult()) {
        doc["result"] = response.result();
    }
    if (response.hasError()) {
        doc["error"] = response.error();
    }

    std::string jsonResponse;
    serializeJson(doc, jsonResponse);
    return jsonResponse;
}

void BLEMCPServer::sendResponse(const std::string& jsonResponse, int httpStatusCode) {
    (void)httpStatusCode; // Not used in BLE
    mcp_transport_send_message(jsonResponse.c_str());
}

void BLEMCPServer::processMessage(const char* message) {
    MCPRequest request = parseRequest(message);
    MCPResponse response = handle(request);
    std::string jsonResponse = serializeResponse(response);
    sendResponse(jsonResponse, response.httpStatusCode);
}

MCPResponse BLEMCPServer::handle(MCPRequest& request) {
    if (request.method.empty()) {
        return createJSONRPCError(static_cast<int>(ErrorCode::PARSE_ERROR), request.id(), "Parse error: Invalid JSON");
    }

    if (request.method == "initialize") {
        return handleInitialize(request);
    } else if (request.method == "tools/list") {
        return handleToolsList(request);
    } else if (request.method == "notifications/initialized") {
        return handleInitialized(request);
    } else if (request.method == "tools/call") {
        return handleFunctionCalls(request);
    } else {
        return createJSONRPCError(static_cast<int>(ErrorCode::METHOD_NOT_FOUND), request.id(), "Method not found: " + request.method);
    }
}

MCPResponse BLEMCPServer::handleInitialize(MCPRequest& request) {
    MCPResponse response(request.id());
    JsonObject result = response.resultDoc.to<JsonObject>();

    result["protocolVersion"] = PROTOCOL_VERSION;

    JsonObject capabilities = result["capabilities"].to<JsonObject>();
    JsonObject experimental = capabilities["experimental"].to<JsonObject>();

    JsonObject tools = capabilities["tools"].to<JsonObject>();
    tools["listChanged"] = false;

    JsonObject serverInfo = result["serverInfo"].to<JsonObject>();
    serverInfo["name"] = serverName;
    serverInfo["version"] = serverVersion;

    if (serverInstructions.length() > 0) {
        result["instructions"] = serverInstructions;
    }
    return response;
}

MCPResponse BLEMCPServer::handleInitialized(MCPRequest& request) {
    ESP_LOGI(TAG, "Client initialized");
    MCPResponse response(request.id());
    response.resultDoc.to<JsonObject>();
    response.httpStatusCode = 202;
    return response;
}

MCPResponse BLEMCPServer::handleToolsList(MCPRequest& request) {
    MCPResponse response(request.id());
    JsonObject result = response.resultDoc.to<JsonObject>();
    JsonArray toolsArray = result["tools"].to<JsonArray>();
    
    for (const auto& kv : tools) {
        const String& key = kv.first;
        const Tool& value = kv.second;
        JsonObject tool = toolsArray.createNestedObject();
        tool["name"] = key;
        tool["description"] = value.description;

        JsonObject inputSchemaObj = tool["inputSchema"].to<JsonObject>();
        value.inputSchema.toJson(inputSchemaObj);

        if (value.outputSchema.type.length() > 0) {
            JsonObject outputSchemaObj = tool["outputSchema"].to<JsonObject>();
            value.outputSchema.toJson(outputSchemaObj);
        }
    }
    return response;
}

MCPResponse BLEMCPServer::handleFunctionCalls(MCPRequest& request) {
    MCPResponse mcpResponse(request.id());
    JsonVariantConst params = request.params();

    if (!params["name"].is<std::string>()) {
        return createJSONRPCError(static_cast<int>(ErrorCode::INVALID_PARAMS), request.id(), "Missing or invalid 'name' parameter");
    }

    std::string functionName = params["name"];
    JsonVariantConst arguments = params["arguments"];

    JsonObject result = mcpResponse.resultDoc.to<JsonObject>();
    JsonArray content = result["content"].to<JsonArray>();

    String functionNameStr = String(functionName.c_str());
    auto toolIt = tools.find(functionNameStr);
    if (toolIt != tools.end()) {
        if (toolIt->second.handler) {
            DynamicJsonDocument argsDoc(4096);
            argsDoc.set(arguments);

            DynamicJsonDocument resultDoc = toolIt->second.handler->call(argsDoc);

            String resultText;
            serializeJson(resultDoc, resultText);

            JsonObject textContent = content.createNestedObject();
            textContent["type"] = "text";
            textContent["text"] = resultText;
            return mcpResponse;
        } else {
            return createJSONRPCError(static_cast<int>(ErrorCode::INTERNAL_ERROR), request.id(),
                                      std::string("Tool handler not initialized: ") + functionNameStr.c_str());
        }
    } else {
        return createJSONRPCError(static_cast<int>(ErrorCode::METHOD_NOT_FOUND), request.id(),
                                  std::string("Method not supported: ") + functionNameStr.c_str());
    }
    return mcpResponse;
}

MCPResponse BLEMCPServer::createJSONRPCError(int code, const JsonVariantConst& id, const std::string& message) {
    MCPResponse response(id);

    response.errorDoc["code"] = code;
    response.errorDoc["message"] = message;

    return response;
}
