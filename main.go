package main

import (
	"bytes"
	"encoding/json"
	"github.com/gotk3/gotk3/gdk"
	"github.com/gotk3/gotk3/gtk"
	"image"
	"image/color"
	"image/jpeg"
	"io/ioutil"
	"math/cmplx"
	"time"
)

func fractal(c complex128, maxIter int) int {
	z := 0 + 0i
	i := 0
	for ; i < maxIter && cmplx.Abs(z) < 2; i++ {
		z = z*z + c
	}
	return i
}

func mandelbrot(width, height, maxIter int, x1, y1, x2, y2 float64, allowDistortion bool, output chan<- []int) {
	yratio = (y2 - y1) / float64(height)
	if allowDistortion {
		xratio = (x2 - x1) / float64(width)
	} else {
		xratio = yratio
	}
	set := make([]int, width*height)
	for i := range set {
		c := complex(x1+xratio*float64(i%width), y1+yratio*float64(i/width))
		set[i] = fractal(c, maxIter)
	}
	output <- set
}

func measureTime(name string, t time.Time) {
	labelTime.SetText(name + time.Since(t).String())
}

func drawMandelbrot() {
	var set []int
	if drawLine || (x1old == x1 && y1old == y1 && x2old == x2 && y2old == y2 && oldwidth == width && oldHeight == height && xratio == xratioold && yratio == yratioold) {
		set = oldSet
	} else {
		defer measureTime("Mandelbrot took ", time.Now())
		channels := make([]chan []int, config.ThreadCount)
		yoffset := y2 - y1
		for i, _ := range channels {
			channels[i] = make(chan []int)
			go mandelbrot(width, height/config.ThreadCount, config.MaxIter, x1, y1+float64(i)*(yoffset/float64(config.ThreadCount)), x2, y2-float64(config.ThreadCount-i-1)*(yoffset/float64(config.ThreadCount)), config.AllowDistortion, channels[i])
		}

		for i, _ := range channels {
			set = append(set, <-channels[i]...)
		}

		oldSet = set
		x1old, y1old, x2old, y2old, xratioold, yratioold = x1, y1, x2, y2, xratio, yratio
	}

	//TODO better colors
	m := image.NewRGBA(image.Rect(0, 0, width, height))
	var col color.RGBA
	for i, c := range set {
		/*
			if c == 300 {
				col = color.RGBA{
					R: 0,
					G: 0,
					B: 0,
					A: 0,
				}
			} else if c > 200 {
				col = color.RGBA{
					R: uint8(c / 4),
					G: uint8(c / 6),
					B: uint8(c / 5),
					A: 0,
				}
			} else if c > 100 {
				col = color.RGBA{
					R: uint8(c / 2),
					G: uint8(c / 2),
					B: uint8(c / 3),
					A: 0,
				}
			} else {
				col = color.RGBA{
					R: uint8(c),
					G: uint8(c * 2),
					B: uint8(float64(c) * 1.5),
					A: 0,
				}
			}
		*/
		col = color.RGBA{
			R: 255 - uint8(float64(c)/float64(config.MaxIter)*255),
			G: 255 - uint8(float64(c)/float64(config.MaxIter)*255),
			B: 255 - uint8(float64(c)/float64(config.MaxIter)*255),
			A: 0,
		}

		m.Set(i%width, i/width, col)
	}
	col = color.RGBA{
		R: 255,
		G: 0,
		B: 0,
		A: 0,
	}

	if drawLine && secondClick {
		m.Set(x1pix, y1pix, col)
	} else if drawLine {
		rect(m, x1pix, y1pix, x2pix, y2pix, col)
		drawLine = false
	}

	buf := new(bytes.Buffer)
	jpeg.Encode(buf, m, nil)
	pixbufLoader, _ := gdk.PixbufLoaderNew()
	pixbufLoader.Write(buf.Bytes())
	pixbuf, _ := pixbufLoader.GetPixbuf()
	pixbufLoader.Close()
	img.SetFromPixbuf(pixbuf)
}

func rect(img *image.RGBA, x1, y1, x2, y2 int, col color.Color) {
	for i := x1; i < x2; i++ {
		img.Set(i, y1, col)
		img.Set(i, y2, col)
	}
	for i := y1; i < y2; i++ {
		img.Set(x1, i, col)
		img.Set(x2, i, col)
	}
}

type Configuration struct {
	AllowDistortion bool
	MaxIter int
	ThreadCount int
}

func (c *Configuration) save(path string) {
	data, _ := json.Marshal(c)
	ioutil.WriteFile(path, data, 0644)
}

func readConfig(path string) *Configuration {
	config := &Configuration{}
	data, err := ioutil.ReadFile(path)
	if err != nil {
		config = &Configuration{
			AllowDistortion: false,
			MaxIter:         1500,
			ThreadCount:     2,
		}
		config.save(path)
		return config
	}
	json.Unmarshal(data, config)
	return config
}

var width, height int                             //Size of the drawingarea, updated when resized
var x1, y1, x2, y2 float64 = -2, -1.25, 0.5, 1.25 //mandelbrot coordinates to render on next draw
var x1old, y1old, x2old, y2old float64            //old coordinates and size so it doesn't have to re-render everything if nothing changes
var oldwidth, oldHeight int
var xratio, yratio, xratioold, yratioold, x1new, y1new float64 //ratio pixel - coordinates and the new top left coordinates that are set on the first click
var secondClick, drawLine bool
var x1pix, y1pix, x2pix, y2pix int //clicked position in pixels on screen for drawing lines
var oldSet []int
var img *gtk.Image
var labelTime *gtk.Label
var config *Configuration = readConfig("config.json")

func main() {
	gtk.Init(nil)

	//// Create Window ////

	win, _ := gtk.WindowNew(gtk.WINDOW_TOPLEVEL)
	win.SetTitle("Mandelbrot")
	win.SetDefaultSize(800, 600)
	win.Connect("destroy", func() {
		gtk.MainQuit()
	})

	//// Create Widgets ////

	box, _ := gtk.BoxNew(gtk.ORIENTATION_VERTICAL, 1)
	buttonbox, _ := gtk.BoxNew(gtk.ORIENTATION_HORIZONTAL, 1)

	buttonStart, _ := gtk.ButtonNewWithLabel("Generate Mandelbrot")
	buttonReset, _ := gtk.ButtonNewWithLabel("Reset")
	labelTime, _ = gtk.LabelNew("")

	eventBox, _ := gtk.EventBoxNew()
	img, _ = gtk.ImageNew()

	//// Signals ////

	buttonStart.Connect("clicked", drawMandelbrot)

	buttonReset.Connect("clicked", func() {
		x1, y1, x2, y2 = -2, -1.25, 0.5, 1.25
		drawMandelbrot()
	})

	eventBox.Connect("size-allocate", func(eventBox *gtk.EventBox) {
		oldwidth = width
		oldHeight = height
		allocation := img.GetAllocation()
		width = allocation.GetWidth()
		height = allocation.GetHeight()
	})

	eventBox.Connect("button-press-event", func(eventBox *gtk.EventBox, event *gdk.Event) {
		secondClick = !secondClick
		eventButton := gdk.EventButtonNewFromEvent(event)
		if secondClick {
			x1new = x1 + xratio*eventButton.X()
			y1new = y1 + yratio*eventButton.Y()

			x1pix = int(eventButton.X())
			y1pix = int(eventButton.Y())

			drawLine = true
			drawMandelbrot()
		} else {
			x2 = x1 + xratio*eventButton.X()
			y2 = y1 + yratio*eventButton.Y()
			x1 = x1new
			y1 = y1new

			x2pix = int(eventButton.X())
			y2pix = int(eventButton.Y())

			drawLine = true
			drawMandelbrot()
		}
	})

	//// Add Widgets to Window ////

	eventBox.Add(img)
	box.PackStart(eventBox, true, true, 0)
	buttonbox.Add(buttonStart)
	buttonbox.Add(buttonReset)
	buttonbox.Add(labelTime)
	box.Add(buttonbox)
	win.Add(box)

	win.ShowAll()

	gtk.Main()
}
