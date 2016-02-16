package main

import (
	"encoding/xml"
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

	xml_file, err := ioutil.ReadFile("rawprogram_unsparse.xml")
	if err != nil {
		panic(err)
	}

	var data Data
	err = xml.Unmarshal(xml_file, &data)
	if err != nil {
		panic(err)
	}

	//fmt.Println(data.Program)

	system_file, err := os.Create("system.img")
	if err != nil {
		panic(err)
	}

	var img_start_sector int64
	for _, program := range data.Program {
		if program.Label == "system" {
			id := strings.Split(strings.Split(program.Filename, "_")[1], ".")[0]
			if id == "1" {
				img_start_sector = program.Start_sector
			}

			length := program.Num_partition_sectors * program.SECTOR_SIZE_IN_BYTES
			start_pos := (program.Start_sector - img_start_sector) * program.SECTOR_SIZE_IN_BYTES
			system_file.Seek(start_pos, 0)

			sparse_file, err := os.Open(program.Filename)
			if err != nil {
				panic(err)
			}

			io.Copy(system_file, sparse_file)
			fmt.Printf("system image id %s, positions is %d, length is %d\n", id, start_pos, length)
		}
	}
}
