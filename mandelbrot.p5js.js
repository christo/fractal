const MAXI = 360; // bail out after this many
const colourScale = 28;
let scaling = 0.003;
let xOffset = 2.6; 
let yOffset = 1.6;

function setup() {
  createCanvas(1400, 1050);
  colorMode(HSB);
  background(0);
  for (let i=0; i<width; i++) {
    for (let j=0; j<height; j++) {
      let u = i * scaling - xOffset;
      let v = j * scaling - yOffset;
      let x = u;
      let y = v;
      let n = 0;
      let xSq = 0;
      let ySq = 0;
      while (xSq + ySq < 4 && n < MAXI) {
          xSq = x * x;
          ySq = y * y;
          y = 2 * x * y + v;
          x = xSq - ySq + u;
          n++;
      }
      let hue = ((n*360*colourScale)/MAXI)%360;
      stroke(color(hue, 100, n == MAXI ? 0 : 100));
      point(i,j);
    }
  }    
}

function draw() {
  
}
