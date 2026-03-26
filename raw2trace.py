#[0]Timestamp,[1]Response,[2]IOType,[3]LUN,[4]Offset,[5]Size
SectorSize = 512
offset = int(1650000000000/SectorSize)
SSD_size = 952  # unit -> G
MAX_sector = SSD_size*1024*1024*1024/SectorSize # 80G
# total_trace = 1#100000000

fd_statistic = open("statistic_part2.txt", 'w+')

step = 100000
filelist = ['LUN3','LUN4','LUN6']
for index in filelist:
    outputfile_simple = "rawfile{}G_simple_{}.txt".format(SSD_size, index)
    fd_out_simple = open(outputfile_simple, 'w+')
    print("output file : "+outputfile_simple)
    
    #outputfile = "rawfile{}G.txt".format(SSD_size)
    #fd_out = open(outputfile, 'w+')
    #print("output file : "+outputfile)

    inputfile = index+".csv"
    fd_in = open(inputfile, 'r')
    print("input file : "+inputfile)

    cnt = 0
    read_cnt = 0
    write_cnt = 0
    read_size = 0
    write_size = 0
    smallest_sector = offset + MAX_sector
    largest_sector = 0
    for line in fd_in:
        #print("{}".format(line))
        split_line = line.split(',')
        if split_line[2] == 'W' or split_line[2] == 'R':
            sector = int(int(split_line[4])/SectorSize)
            length = int(int(split_line[5])/SectorSize)
            if sector+length < offset+MAX_sector and sector >= offset:
                cnt += 1
                if split_line[2] == 'W':
                    write_cnt += 1
                    write_size += length
                if split_line[2] == 'R':
                    read_cnt += 1
                    read_size += length
                fd_out_simple.write("{} {} {} {}\n".format(
                    0, split_line[2], sector-offset, length))
                #fd_out.write("{} {} {} {} {}\n".format(int(float(split_line[0])*1000000), 0, sector, length, split_line[2]))
                if sector < smallest_sector:
                    smallest_sector = sector
                if sector+length > largest_sector:
                    largest_sector = sector+length
                if cnt % step == 0:
                    print("{} ".format(cnt))
                # if cnt == total_trace:
                    # break
    fd_in.close()
    fd_out_simple.close()
    print("tatal trace : {}".format(cnt))
    fd_statistic.write("rawfile{}G_simple_{}.txt\n\ttotal requests: {}\n\tread requests: {}\n\twrite requests: {}\n\twrite ratio: {}\n\tread size: {} GB\n\twrite size: {} GB\n\trange: {} GB\n\n".format(
        SSD_size, index, cnt, read_cnt, write_cnt, write_cnt/cnt, read_size*SectorSize/1024/1024/1024, write_size*SectorSize/1024/1024/1024, (largest_sector-smallest_sector)*SectorSize/1024/1024/1024))
fd_statistic.close()