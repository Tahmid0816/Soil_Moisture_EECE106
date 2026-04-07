function doGet(e) { 
  var ss = SpreadsheetApp.getActiveSpreadsheet();
  var sheet = ss.getActiveSheet();
  
  var freq = Number(e.parameter.freq); // Convert to number for math
  var p2p = e.parameter.p2p;
  
  // 1. Calculate exactly which row this frequency belongs in
  // Logic: 5000Hz -> Row 2, 15000Hz -> Row 3, 25000Hz -> Row 4, etc.
  var targetRow = ((freq - 5000) / 10000) + 2;
  
  // 2. If it's the start of a sweep, clear the old data area first
  if (freq === 5000) {
    sheet.getRange("A2:C11").clearContent();
  }
  
  // 3. Write data to the SPECIFIC calculated row instead of appending
  // getRange(row, column, numRows, numColumns)
  sheet.getRange(targetRow, 1, 1, 3).setValues([[new Date(), freq, p2p]]);
  
  SpreadsheetApp.flush(); 
  return ContentService.createTextOutput("Data Received for Row " + targetRow);
}
