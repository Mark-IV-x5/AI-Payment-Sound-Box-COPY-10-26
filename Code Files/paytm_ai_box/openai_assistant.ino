// ******************************************************************************************************************************
// OPENAI - Date interpretation for Razorpay queries + JSON extraction helper
// ******************************************************************************************************************************

String OpenAI_RazorpayAssistant(String UserRequest, String currentDate) {
  if (UserRequest == "") return "{\"error\":\"Empty input\"}";

  UserRequest.replace("\"", "\\\"");
  UserRequest.replace("\n", " ");
  UserRequest.replace("\r", "");

  String systemPrompt = "{\"role\":\"system\",\"content\":\"";
  systemPrompt += "You are a Razorpay payment assistant. Convert the user's question into a structured JSON action.\\n\\n";
  systemPrompt += "CURRENT DATE: " + currentDate + "\\n\\n";
  systemPrompt += "ACTIONS (return EXACTLY one):\\n";
  systemPrompt += "1) range_total - total amount in a date range. Use for: 'today', 'yesterday', 'last 10 days', 'this/last month', 'from X to Y'.\\n";
  systemPrompt += "   {\\\\\\\"action\\\\\\\":\\\\\\\"range_total\\\\\\\",\\\\\\\"from_year\\\\\\\":Y,\\\\\\\"from_month\\\\\\\":M,\\\\\\\"from_day\\\\\\\":D,\\\\\\\"to_year\\\\\\\":Y,\\\\\\\"to_month\\\\\\\":M,\\\\\\\"to_day\\\\\\\":D}\\n";
  systemPrompt += "2) last_payment - the single most recent captured payment. Use for: 'last payment', 'latest transaction', 'how much was the last one'.\\n";
  systemPrompt += "   {\\\\\\\"action\\\\\\\":\\\\\\\"last_payment\\\\\\\"}\\n";
  systemPrompt += "3) last_n - the last N captured payments (sum + count). Use for: 'last 5 payments', 'recent 3 transactions'.\\n";
  systemPrompt += "   {\\\\\\\"action\\\\\\\":\\\\\\\"last_n\\\\\\\",\\\\\\\"n\\\\\\\":5}\\n";
  systemPrompt += "4) count_range - number of payments in a date range. Use for: 'how many transactions today', 'payment count this month'.\\n";
  systemPrompt += "   {\\\\\\\"action\\\\\\\":\\\\\\\"count_range\\\\\\\",\\\\\\\"from_year\\\\\\\":Y,\\\\\\\"from_month\\\\\\\":M,\\\\\\\"from_day\\\\\\\":D,\\\\\\\"to_year\\\\\\\":Y,\\\\\\\"to_month\\\\\\\":M,\\\\\\\"to_day\\\\\\\":D}\\n\\n";
  systemPrompt += "If unclear: {\\\\\\\"action\\\\\\\":\\\\\\\"error\\\\\\\",\\\\\\\"message\\\\\\\":\\\\\\\"Unable to understand\\\\\\\"}\\n\\n";
  systemPrompt += "Return JSON only, no explanations. Questions may be in English or Hindi.\"}";

  String request_body = "{\"model\":\"gpt-4o-mini\",\"messages\":[";
  request_body += systemPrompt;
  request_body += ",{\"role\":\"user\",\"content\":\"" + UserRequest + "\"}],";
  request_body += "\"temperature\":0,\"max_tokens\":200}";

  WiFiClientSecure client_tcp;
  client_tcp.setInsecure();
  if (!client_tcp.connect("api.openai.com", 443)) {
    Serial.println("Connection to OpenAI failed!");
    return "{\"error\":\"Connection failed\"}";
  }

  client_tcp.println("POST /v1/chat/completions HTTP/1.1");
  client_tcp.println("Host: api.openai.com");
  client_tcp.println("Authorization: Bearer " + cfg_openai_key);
  client_tcp.println("Content-Type: application/json; charset=utf-8");
  client_tcp.println("Content-Length: " + String(request_body.length()));
  client_tcp.println("Connection: close");
  client_tcp.println();
  client_tcp.print(request_body);

  String LLM_Response = "";
  uint32_t t_start = millis();
  while (client_tcp.connected() || client_tcp.available()) {
    if (millis() - t_start > 20000) {
      client_tcp.stop();
      return "{\"error\":\"Timeout\"}";
    }
    while (client_tcp.available()) {
      LLM_Response += (char)client_tcp.read();
    }
    delay(10);
  }
  client_tcp.stop();

  return extractJSON(LLM_Response);
}

String extractJSON(String response) {
  DynamicJsonDocument env(4096);
  DeserializationError err = deserializeJson(env, response);
  if (err) {
    DebugPrintln("Envelope parse error: " + String(err.c_str()));
    return "{\"error\":\"Invalid envelope\"}";
  }

  const char* content = env["choices"][0]["message"]["content"];
  if (!content) {
    DebugPrintln("No content field in response");
    return "{\"error\":\"No content in response\"}";
  }

  String c = String(content);
  c.trim();
  int b = c.indexOf('{');
  int e = c.lastIndexOf('}');
  if (b >= 0 && e > b) return c.substring(b, e + 1);
  return c;
}
