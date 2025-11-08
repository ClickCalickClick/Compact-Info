module.exports = [
  {
    "type": "heading",
    "defaultValue": "Compact Info Settings"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Display"
      },
      {
        "type": "toggle",
        "messageKey": "InvertColors",
        "label": "Invert Colors",
        "description": "Black background, white text",
        "defaultValue": false
      },
      {
        "type": "radiogroup",
        "messageKey": "TimeFormat",
        "label": "Time Format",
        "defaultValue": "0",
        "options": [
          {
            "label": "Words (e.g., SEVEN FIFTY SIX)",
            "value": "0"
          },
          {
            "label": "12-Hour (e.g., 7:56 PM)",
            "value": "1"
          },
          {
            "label": "24-Hour (e.g., 19:56)",
            "value": "2"
          }
        ]
      },
      {
        "type": "color",
        "messageKey": "ColorTheme",
        "defaultValue": "0x000000",
        "label": "Color Theme",
        "description": "Only applies to Compact Info design",
        "sunlight": false,
        "allowGray": false,
        "capabilities": ["COLOR"]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Sections"
      },
      {
        "type": "toggle",
        "messageKey": "ShowDate",
        "label": "Show Date",
        "description": "Display current date",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "ShowWeather",
        "label": "Show Weather",
        "description": "Display weather information",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "ShowBattery",
        "label": "Show Battery",
        "description": "Display battery status",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Weather"
      },
      {
        "type": "toggle",
        "messageKey": "TemperatureUnit",
        "label": "Use Celsius",
        "description": "Temperature in Celsius instead of Fahrenheit",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "UseGPS",
        "label": "Auto Location (GPS)",
        "description": "Use phone's GPS for weather location",
        "defaultValue": true
      },
      {
        "type": "input",
        "messageKey": "ZipCode",
        "defaultValue": "",
        "label": "ZIP Code / City",
        "description": "Enter ZIP code or city name (used when GPS is off)",
        "attributes": {
          "placeholder": "e.g., 90210 or London"
        }
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
