//
//  DuckDB
//  https://github.com/duckdb/duckdb-swift
//
//  Copyright © 2018-2023 Stichting DuckDB Foundation
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to
//  deal in the Software without restriction, including without limitation the
//  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
//  sell copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
//  IN THE SOFTWARE.

@_implementationOnly import Cduckdb
import Foundation

final class LogicalType {
  
  private let ptr = UnsafeMutablePointer<duckdb_logical_type?>.allocate(capacity: 1)
  
  init(_ body: () -> duckdb_logical_type?) {
    self.ptr.pointee = body()
  }
  
  deinit {
    duckdb_destroy_logical_type(ptr)
    ptr.deallocate()
  }
  
  var dataType: DatabaseType {
    let ctypeid = duckdb_get_type_id(ptr.pointee)
    return ctypeid.asTypeID
  }
  
  var underlyingDataType: DatabaseType {
    guard dataType == .enum else { return dataType }
    let ctypeid = duckdb_enum_internal_type(ptr.pointee)
    return ctypeid.asTypeID
  }
}

// MARK: - Decimal

extension LogicalType {
  
  struct DecimalProperties {
    let width: UInt8
    let scale: UInt8
    let storageType: DatabaseType
  }
  
  var decimalProperties: DecimalProperties? {
    guard dataType == .decimal else { return nil }
    let internalStorageType = duckdb_decimal_internal_type(ptr.pointee)
    return .init(
      width: duckdb_decimal_width(ptr.pointee),
      scale: duckdb_decimal_scale(ptr.pointee),
      storageType: internalStorageType.asTypeID
    )
  }
}

// MARK: - Struct

extension LogicalType {
  
  static let structCompatibleTypes = [DatabaseType.struct, .map]
  
  var structMemberNames: [String]? {
    guard Self.structCompatibleTypes.contains(dataType) else { return nil }
    let memberCount = duckdb_struct_type_child_count(ptr.pointee)
    var names = [String]()
    for i in 0..<memberCount {
      let cStr = duckdb_struct_type_child_name(ptr.pointee, i)!
      names.append(String(cString: cStr))
      duckdb_free(cStr)
    }
    return names
  }
}
