FROM nodered/node-red:4.1.17-22

WORKDIR /usr/src/node-red
RUN npm install --no-audit --no-fund --omit=dev @flowfuse/node-red-dashboard@1.34.1

COPY settings.js /usr/src/node-red/settings.js

ENV FLOWS=/flows/flows.json

CMD ["--settings", "/usr/src/node-red/settings.js"]
