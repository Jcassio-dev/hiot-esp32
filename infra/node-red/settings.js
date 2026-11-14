const crypto = require("crypto");
const fs = require("fs");
const path = require("path");

const flowFile = process.env.NODE_RED_FLOW_FILE || "/flows/flows.json";
const userDir = process.env.NODE_RED_USER_DIR || "/data";
const credentialSecret = process.env.NODE_RED_CREDENTIAL_SECRET;

// gera o flows_cred.json com usuário/senha do broker vindos do .env
const credFile = path.join(userDir, path.basename(flowFile, ".json") + "_cred.json");
if (fs.existsSync(flowFile)) {
    const credentials = {};
    for (const node of JSON.parse(fs.readFileSync(flowFile, "utf8"))) {
        if (node.type === "mqtt-broker") {
            credentials[node.id] = { user: "${MQTT_USER}", password: "${MQTT_PASS}" };
        }
    }
    const creds = JSON.stringify(credentials);
    const key = crypto.createHash("sha256").update(credentialSecret).digest();
    const iv = crypto.randomBytes(16);
    const cipher = crypto.createCipheriv("aes-256-ctr", key, iv);
    const data = cipher.update(creds, "utf8", "base64") + cipher.final("base64");
    fs.writeFileSync(credFile, JSON.stringify({ $: iv.toString("hex") + data }, null, 4));
}

module.exports = {
    uiPort: 1880,

    flowFile,
    flowFilePretty: true,
    credentialSecret,

    adminAuth: {
        type: "credentials",
        users: [{
            username: process.env.NODE_RED_ADMIN_USER || "admin",
            password: process.env.NODE_RED_ADMIN_HASH,
            permissions: "*"
        }]
    },

    contextStorage: {
        default: { module: "memory" },
        file: { module: "localfilesystem", config: { flushInterval: 5 } }
    },

    functionExternalModules: false,
    exportGlobalContextKeys: false,

    logging: {
        console: { level: "info", metrics: false, audit: false }
    },

    editorTheme: {
        projects: { enabled: false },
        tours: false
    }
};
