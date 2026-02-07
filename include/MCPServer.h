#ifndef MCP_SERVER_BLE_H
#define MCP_SERVER_BLE_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include <map>
#include <memory>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

const char* const PROTOCOL_VERSION = "2024-11-05";
const char* const DEFAULT_SERVER_NAME = "ESP32-MCP-BLE";
const char* const DEFAULT_SERVER_VERSION = "1.0.0";

struct MCPRequest {
    std::string method;
    DynamicJsonDocument idDoc;
    DynamicJsonDocument paramsDoc;

    MCPRequest() : method(""), idDoc(256), paramsDoc(4096) {}

    JsonVariantConst params() const {
        return paramsDoc.as<JsonVariantConst>();
    }

    JsonVariantConst id() const {
        return idDoc.as<JsonVariantConst>();
    }

    bool hasParams() const {
        return !paramsDoc.isNull();
    }
};

struct MCPResponse {
    DynamicJsonDocument idDoc;
    DynamicJsonDocument resultDoc;
    DynamicJsonDocument errorDoc;
    int httpStatusCode;

    MCPResponse() : idDoc(256), resultDoc(8192), errorDoc(512), httpStatusCode(200) {}
    MCPResponse(const JsonVariantConst& id) : idDoc(256), resultDoc(8192), errorDoc(512), httpStatusCode(200) {
        idDoc.set(id);
    }

    JsonVariantConst id() const {
        return idDoc.as<JsonVariantConst>();
    }
    JsonVariantConst result() const {
        return resultDoc.as<JsonVariantConst>();
    }
    JsonVariantConst error() const {
        return errorDoc.as<JsonVariantConst>();
    }

    bool hasResult() const {
        return !resultDoc.isNull();
    }
    bool hasError() const {
        return !errorDoc.isNull();
    }
};

enum class ErrorCode {
    SERVER_ERROR = -32000,
    INVALID_REQUEST = -32600,
    METHOD_NOT_FOUND = -32601,
    INVALID_PARAMS = -32602,
    INTERNAL_ERROR = -32603,
    PARSE_ERROR = -32700
};

class ToolHandler {
   public:
    virtual ~ToolHandler() = default;
    virtual DynamicJsonDocument call(const DynamicJsonDocument& params) = 0;
};

class Properties {
   public:
    Properties() = default;

    Properties(const Properties& other) {
        type = other.type;
        title = other.title;
        description = other.description;
        properties = other.properties;
        required = other.required;
        additionalProperties = other.additionalProperties;
        hasAdditionalProperties = other.hasAdditionalProperties;
        if (other.items) {
            items.reset(new Properties(*other.items));
        } else {
            items.reset(nullptr);
        }
        enumValues = other.enumValues;
        oneOf = other.oneOf;
        anyOf = other.anyOf;
        allOf = other.allOf;
        format = other.format;
        defaultValue = other.defaultValue;
    }

    Properties& operator=(const Properties& other) {
        if (this != &other) {
            type = other.type;
            title = other.title;
            description = other.description;
            properties = other.properties;
            required = other.required;
            additionalProperties = other.additionalProperties;
            hasAdditionalProperties = other.hasAdditionalProperties;
            if (other.items) {
                items.reset(new Properties(*other.items));
            } else {
                items.reset(nullptr);
            }
            enumValues = other.enumValues;
            oneOf = other.oneOf;
            anyOf = other.anyOf;
            allOf = other.allOf;
            format = other.format;
            defaultValue = other.defaultValue;
        }
        return *this;
    }

    Properties(Properties&&) = default;
    Properties& operator=(Properties&&) = default;

    String type;
    String title;
    String description;
    std::map<String, Properties> properties;
    std::vector<String> required;

    bool additionalProperties = true;
    bool hasAdditionalProperties = false;

    std::unique_ptr<Properties> items;

    std::vector<String> enumValues;

    std::vector<Properties> oneOf;
    std::vector<Properties> anyOf;
    std::vector<Properties> allOf;

    String format;

    String defaultValue;

    String toString() const;
    void toJson(JsonObject& obj) const;
};

class Tool {
   public:
    Tool() = default;
    Tool(const Tool&) = default;
    Tool& operator=(const Tool&) = default;
    Tool(Tool&&) = default;
    Tool& operator=(Tool&&) = default;

    String name;
    String description;
    Properties inputSchema;
    Properties outputSchema;
    std::shared_ptr<ToolHandler> handler;

    String toString() const;
};

class BLEMCPServer {
   public:
    BLEMCPServer(const String& name = DEFAULT_SERVER_NAME, const String& version = DEFAULT_SERVER_VERSION,
                 const String& instructions = "");
    
    void RegisterTool(const Tool& tool);
    void begin();
    void loop();

   private:
    static void onMessage(const char* message, void* ctx);
    void processMessage(const char* message);
    
    std::string serializeResponse(const MCPResponse& response);
    void sendResponse(const std::string& jsonResponse, int httpStatusCode);

    MCPRequest parseRequest(const std::string& json);

    MCPResponse createJSONRPCError(int code, const JsonVariantConst& id, const std::string& message);
    MCPResponse handle(MCPRequest& request);
    MCPResponse handleInitialize(MCPRequest& request);
    MCPResponse handleInitialized(MCPRequest& request);
    MCPResponse handleToolsList(MCPRequest& request);
    MCPResponse handleFunctionCalls(MCPRequest& request);

    // BLE Transport members
    static void taskEntry(void* ctx);
    static int sendBytes(const uint8_t* data, size_t len, void* ctx);
    static void onMtu(uint16_t mtu);
    static void sleepTicks(uint32_t ticks, void* ctx);
    static void logFn(int level, const char* tag, const char* message, void* ctx);

    QueueHandle_t rx_queue = nullptr;
    TaskHandle_t task_handle = nullptr;

    static BLEMCPServer* s_bound;
    static bool s_initialized;

   private:
    std::map<String, Tool> tools;
    String serverName;
    String serverVersion;
    String serverInstructions;
};

#endif  // MCP_SERVER_BLE_H
