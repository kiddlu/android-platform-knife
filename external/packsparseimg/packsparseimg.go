package main

import (
	"encoding/xml"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"strings"
)

type Data struct {
	Program []Program `xml:"program"`
}

type Program struct {
	SECTOR_SIZE_IN_BYTES      int64  `xml:"SECTOR_SIZE_IN_BYTES,attr"`
	File_sector_offset        int64  `xml:"file_sector_offset,attr"`
	Filename                  string `xml:"filename,attr"`
	Label                     string `xml:"label,attr"`
	Num_partition_sectors     int64  `xml:"num_partition_sectors,attr"`
	Physical_partition_number int64  `xml:"physical_partition_number,attr"`
	Size_in_KB                string `xml:"size_in_KB,attr"`
	Sparse                    string `xml:"sparse,attr"`
	Start_byte_hex            string `xml:"start_byte_hex,attr"`
	Start_sector              int64  `xml:"start_sector,attr"`
}

func main() {

	var xml_path, output_path string
	var image string
	flag.StringVar(&xml_path, "x", "./rawprogram_unsparse.xml", "xml file to load")
	flag.StringVar(&output_path, "o", "./", "output dir path")
	flag.StringVar(&image, "t", "system", "image to unsparse: system / cache / userdata")
	flag.Parse()

	switch image {
	case "system", "cache", "userdata":
		fmt.Printf("target image is %s\n", image)
	default:
		panic("target image must be system / cache / userdata")
	}
	fmt.Printf("xml_path is %s\n", xml_path)
	fmt.Printf("output_path is %s\n", output_path)

	xml_file, err := ioutil.ReadFile(xml_path)
	if err != nil {
		panic(err)
	}

	var data Data
	err = xml.Unmarshal(xml_file, &data)
	if err != nil {
		panic(err)
	}
	//fmt.Println(data.Program)

	/*
		target := []string{"system", "cache", "userdata"}
		for _, image := range target {
		}
	*/

	var img_start_sector int64
	var length int64
	var start_pos int64
//	var last_pos, padding int64

	img_file, err := os.Create(output_path + "/" + image + ".raw")
	if err != nil {
		panic(err)
	}

	for _, program := range data.Program {
		if program.Label == image {
			id := strings.Split(strings.Split(program.Filename, "_")[1], ".")[0]
			if id == "1" {
				img_start_sector = program.Start_sector
			}
			length = program.Num_partition_sectors * program.SECTOR_SIZE_IN_BYTES
			start_pos = (program.Start_sector - img_start_sector) * program.SECTOR_SIZE_IN_BYTES
			img_file.Seek(start_pos, os.SEEK_SET)

			sparse_file, err := os.Open(program.Filename)
			if err != nil {
				panic(err)
			}
			io.Copy(img_file, sparse_file)
			fmt.Printf("%s image id %s, positions is %d, length is %d\n", image, id, start_pos, length)
		}
	}
//	padding = 128 * 1024 * 1024
//	last_pos = padding + start_pos + length - 1
//	img_file.Seek(last_pos, os.SEEK_SET)
//	end_byte := []byte{0x00}
//	img_file.Write(end_byte)
	fmt.Printf("generate %s/%s.raw compelet", output_path, image)
}
